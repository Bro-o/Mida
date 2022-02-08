#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

#include "../0_headers/drive.h"
#include "../0_headers/tracefiles.h"

class CountDrive : public Drive_t<Page> {
 protected:
  virtual streamno_t gc_streamid_policy(addr_t addr, streamno_t old_streamid) {
    // old_streamid + 1, unless it goes over
    return std::min(old_streamid + 1, numberofstreams - 1);
  }

  virtual streamno_t new_streamid_policy(addr_t addr, streamno_t given) {
    // start with 0
    return 0;
  }

 public:
  using Drive_t::Drive_t;
};

int main(int argc, const char* argv[]) {
  if (argc < 5) {
    std::cout << "Usage: countGC <max lba> <OPS ratio> <# of streams> "
              << "<traces>\n";
    return 0;
  }

  addr_t maxlba = std::stoul(argv[1]);
  double ops = std::stod(argv[2]);
  streamno_t numberofstreams = std::stoul(argv[3]);
  CountDrive drive(maxlba, ops, numberofstreams);

  for (int ii = 4; ii < argc; ++ii) {
    Trace t(argv[ii]);
    drive.run(t);
    drive.printstat();
    drive.printresult();
    drive.resetstat();
  }

  return 0;
}
