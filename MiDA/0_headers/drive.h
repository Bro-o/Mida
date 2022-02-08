#ifndef DRIVE_H
#define DRIVE_H

#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <numeric>
#include <cmath>

#include "tracefiles.h"

struct Page {
  blockno_opt blockno = std::nullopt;
  streamno_opt streamid = std::nullopt;
  vtime_opt written_time = std::nullopt;

  virtual void write(blockno_t _blockno, streamno_t _streamid, vtime_t _vtime) {
    blockno = _blockno;
    streamid = _streamid;
    written_time = _vtime;
  }

  virtual void move(blockno_t _blockno, streamno_t _streamid) {
    blockno = _blockno;
    streamid = _streamid;
  }

  virtual void trim() {
    assert(blockno);
    assert(streamid);
    assert(written_time);

    blockno = std::nullopt;
    streamid = std::nullopt;
    written_time = std::nullopt;
  }
};

struct stat_t {
  std::vector<vtime_t> list;

  void add_stat(vtime_t vtime, vtime_t written_time) {
    assert(written_time <= vtime);
    list.push_back(vtime - written_time);
  }

  void reset() {
    list.clear();
  }
};

struct Block {
  const static std::size_t ppb = 128;
  const blockno_t blockno;
  count_t valid = 0;
  count_t invalid = 0;
  bool writing = false;
  std::unordered_set<addr_t> addrs;
  count_t gc_count = 0;

  Block(blockno_t _blockno) : blockno(_blockno){};

  void write(addr_t addr, streamno_t streamid) {
    assert(writable());

    valid += 1;
    addrs.insert(addr);

    if (!writable()) {
      writing = false;
    }
  }

  void trim(addr_t addr) {
    valid -= 1;
    if (valid == 0) {
      invalid = 0;
    } else {
      invalid += 1;
      assert(valid + invalid <= ppb);
    }
    addrs.erase(addr);
  }

  void clear() {
    valid = 0;
    invalid = 0;
    writing = false;
    addrs.clear();
  }

  bool empty() const { return (valid == 0); }
  bool writable() const { return (writing && (valid + invalid < ppb)); }
};

template <class Page_t>
class Drive_t {
 protected:
  addr_t lba_size;         // size of lba space, excl. ops
  pageno_t numberofpages;  // pages including ops
  blockno_t numberofblocks;
  streamno_t numberofstreams;

  vtime_t vtime_total = 0;
  vtime_t vtime = 0;
  count_t pagescopied = 0;
  count_t writes = 0;
  count_t trims = 0;

  blockno_t gc_threshold;

  std::vector<Page_t> pages;
  std::vector<Block> blocks;
  std::vector<Block*> cursors;
  std::queue<Block*> emptyblocks;

  std::vector<stat_t> stats;

  Block* get_victim() {
    count_t min_valid = Block::ppb;
    Block* victim = nullptr;

    for (Block& blk : blocks) {
      if (!blk.writing && !blk.empty() && blk.valid < min_valid) {
        min_valid = blk.valid;
        victim = &blk;
      }
    }

    assert(victim);
    assert(!victim->writing);
    assert(!victim->empty());
    assert(victim->valid != Block::ppb);
    return victim;
  }

  Block* get_empty_block() {
    Block* ret = emptyblocks.front();
    emptyblocks.pop();
    return ret;
  }

  Block* new_cursor() {
    Block* blk = get_empty_block();
    assert(blk);
    assert(blk->valid == 0);
    assert(blk->invalid == 0);
    assert(!blk->writing);
    blk->writing = true;
    return blk;
  }

  void move_page(addr_t addr, streamno_t streamid) {
    Block*& cursor = cursors.at(streamid);
    if (!cursor || !cursor->writable()) {
      cursor = new_cursor();
    }

    cursor->write(addr, streamid);
    pages.at(addr).move(cursor->blockno, streamid);
  }

  void collect_garbage() {
    // std::cout << "gc\n";
    Block* victim = get_victim();
    assert(victim);

    pagescopied += victim->valid;
    victim->gc_count += 1;

    auto addrs = victim->addrs;  // copy since we will trim and change it
    for (addr_t addr : addrs) {
      streamno_t old_streamid = *pages.at(addr).streamid;
      move_page(addr, gc_streamid_policy(addr, old_streamid));
    }
    victim->clear();
    emptyblocks.push(victim);
  }

  virtual streamno_t gc_streamid_policy(addr_t addr, streamno_t old_streamid) {
    assert(old_streamid < numberofstreams);
    return old_streamid;
  }

  virtual streamno_t new_streamid_policy(addr_t addr, streamno_t given) {
    return std::min(given, numberofstreams - 1);
  }

 public:
  Drive_t(addr_t _lba_size, double ops, streamno_t _numberofstreams) {
    lba_size = _lba_size;
    numberofpages = pageno_t(_lba_size * (1 + ops / 100.0) + 1);
    numberofstreams = _numberofstreams;

    numberofblocks = numberofpages / Block::ppb;
    gc_threshold = std::max(blockno_t(0.05 * numberofblocks), numberofstreams);
    if (numberofblocks < lba_size / Block::ppb + gc_threshold) {
      std::cout << "Warning: ops too small\n";
    }

    for (blockno_t ii = 0; ii < numberofblocks; ++ii) {
      blocks.emplace_back(ii);
    }

    for (blockno_t ii = 0; ii < numberofblocks; ++ii) {
      emptyblocks.push(&blocks.at(ii));
    }

    pages.resize(numberofpages);
    cursors.resize(numberofstreams);
    stats.resize(_numberofstreams);

    std::cout << "logical device size: " << lba_size / 244140.62 << "GB ("
              << lba_size / 262144.0 << "GiB)\n";
    std::cout << "physical device size: " << numberofpages / 244140.62 << "GB ("
              << numberofpages / 262144.0 << "GiB)\n";
    std::cout << "OPS: " << (numberofpages - lba_size) * 100.0 / lba_size
              << "%\n";
    std::cout << "number of streams: " << numberofstreams << std::endl;

    write_all_seq();
  }

  void write_all_seq() {
    for (addr_t addr = 0; addr < lba_size; ++addr) {
      vtime_total += 1;
      vtime += 1;

      write(addr, new_streamid_policy(addr, 0));

      if (VERBOSE && vtime % 100000 == 0) {
        show_progress(lba_size);
      }
    }

    resetstat();

    if (VERBOSE) {
      std::cout << '\n';
    }

    std::cout << "--------------------------------\n";
  }

  void run(Trace& trace) {
    std::cout << "trace length: " << trace.length() << " ("
              << trace.length() * 4 / 1000000.0 << "GB, "
              << trace.length() * 4 / 1048576.0 << "GiB)" << std::endl;

    while (Line line = trace.readtrace()) {
      auto [addr, length, streamid, is_trim] = *line;
      auto fin = addr + length;
      for (; addr < lba_size && addr < fin; ++addr) {
        vtime_total += 1;
        vtime += 1;

        trim(addr);

        if (!is_trim) {
          assert(streamid);
          write(addr, new_streamid_policy(addr, *streamid));
        } else {
          trims += 1;
        }

        if (VERBOSE && vtime % 100000 == 0) {
          show_progress(trace.length());
        }
      }

      // if (vtime > 1000000) {
      //   break;
      // }
    }

    if (VERBOSE) {
      std::cout << '\n';
    }
  }

  virtual void trim(addr_t addr) {
    if (addr >= lba_size) {
      return;
    }

    auto& p = pages.at(addr);
    if (p.blockno == std::nullopt) {
      return;
    }

    assert(p.streamid);
    assert(p.written_time);

    Block* blk = &blocks.at(*p.blockno);
    blk->trim(addr);
    if (blk->empty() && !blk->writing) {
      emptyblocks.push(blk);
    }

    p.trim();

    stats.at(*p.streamid).add_stat(vtime_total, *p.written_time);
  }

  virtual void write(addr_t addr, streamno_t stream) {
    if (addr >= lba_size) {
      return;
    }

    writes += 1;

    assert(stream < numberofstreams);
    Block*& cursor = cursors.at(stream);

    if (!cursor || !cursor->writable()) {
      cursor = new_cursor();
    }

    cursor->write(addr, stream);
    pages.at(addr).write(cursor->blockno, stream, vtime_total);

    while (emptyblocks.size() < gc_threshold) {
      collect_garbage();
    }
  }

  virtual void show_progress(tracelen_t trace_length) {
    std::cerr << vtime << " / " << trace_length << ": " << waf()
              << "          \r" << std::flush;
    // std::cout << vtime << ": " << waf() << "          " << std::endl;
  }

  void printresult() {
    std::cout << "vtime: " << vtime << '\n';
    std::cout << "writes: " << writes << '\n';
    std::cout << "trims: " << trims << '\n';
    std::cout << "pagescopied: " << pagescopied << '\n';
    std::cout << "WAF: " << waf() << '\n';
    std::cout << "--------------------------------\n";
  }

  double waf() {
    if (writes == 0) {
      return 0;
    } else {
      return double(pagescopied) / double(writes) + 1.0;
    }
  }

  void printstat() {
    for (std::size_t ii = 0; ii < stats.size(); ++ii) {
      auto& list = stats.at(ii).list;
      double average = std::accumulate(list.begin(), list.end(), 0.0) / list.size();

      double sq_sum = std::inner_product(list.begin(), list.end(), list.begin(), 0.0);
      double stdev = std::sqrt(sq_sum / list.size() - average * average);

      if (list.size() > 0) {
        std::cout << "stream " << ii << " ";
        std::cout << "average " << int(average) << " ";
        std::cout << "stdev " << int(stdev) << std::endl;
      }
    }

    // for (std::size_t ii = 0; ii < stats.size(); ++ii) {
    //   auto& list = stats.at(ii).list;
    //   for (vtime_t t: list) {
    //     std::cout << t << ' ';
    //   }
    //   std::cout << std::endl;
    // }
  }

  void resetstat() {
    vtime = 0;
    pagescopied = 0;
    writes = 0;

    for (stat_t stat : stats) {
      stat.reset();
    }
  }
};

class Drive : public Drive_t<Page> {
  using Drive_t::Drive_t;
};

#endif  // DRIVE_H
