#include <iostream>
#include <string>

#include "../0_headers/drive.h"

int main(int argc, const char* argv[]) {
  if (argc < 5) {
    std::cout << "Usage: manual <max lba> <OPS ratio> <# of streams> "
              << "<traces>\n";
    return 0;
  }

  addr_t maxlba = std::stoul(argv[1]);
  double ops = std::stod(argv[2]);
  streamno_t numberofstreams = std::stoul(argv[3]);
  Drive drive(maxlba, ops, numberofstreams);

  for (int ii = 4; ii < argc; ++ii) {
    Trace t(argv[ii]);
    drive.run(t);
    drive.printstat();
    drive.printresult();
    drive.resetstat();
  }

  return 0;
}
