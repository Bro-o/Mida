#include <iostream>
#include <optional>

using streamno_t = std::size_t;
using addr_t = std::size_t;
using pageno_t = std::size_t;
using blockno_t = std::size_t;
using vtime_t = std::size_t;
using count_t = std::size_t;
using tracelen_t = std::size_t;

using streamno_opt = std::optional<streamno_t>;
using addr_opt = std::optional<addr_t>;
using pageno_opt = std::optional<pageno_t>;
using blockno_opt = std::optional<blockno_t>;
using vtime_opt = std::optional<vtime_t>;
using count_opt = std::optional<count_t>;
using tracelen_opt = std::optional<tracelen_t>;

struct Line_t {
  addr_t addr;
  tracelen_t length;
  streamno_opt streamid;
  bool trim;
};

using Line = std::optional<Line_t>;