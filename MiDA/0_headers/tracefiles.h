#ifndef TRACEFILES_H
#define TRACEFILES_H

#include <fstream>
#include <iostream>

#include "types.h"

class Trace {
  std::fstream tracefile;
  tracelen_t _length;
  addr_t _max_lba;

 public:
  Trace(const char* f1) {
    tracefile = std::fstream(f1);
    tracefile >> _length >> _max_lba;
  }

  Line readtrace() {
    addr_t addr;
    tracelen_t write_length;
    streamno_opt streamid;
    int _streamid;

    if (tracefile >> addr >> write_length >> _streamid) {
      streamid = _streamid < 0 ? std::nullopt : streamno_opt(_streamid);
      return Line(Line_t{addr, write_length, streamid, (_streamid < 0)});
    } else {
      return std::nullopt;
    }
  }

  void rewind() {
    tracefile.clear();
    tracefile.seekg(0);

    tracefile >> _length >> _max_lba;
  }

  tracelen_t length() { return _length; }

  addr_t max_lba() { return _max_lba; }
};

#endif  // TRACEFILES_H
