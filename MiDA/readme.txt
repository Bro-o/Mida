1. build
 - bash build 3_run/mida.cpp
2. run
 - ./3_run/mida.exe <total number of pages> <OPS ratio> <# of separation groups> <trace file>
 - ex) ./3_run/mida.exe 7812608 7.3727 6 ycsb.txt
 - above example runs 32GB drive with 32GiB internal space with 6 separation groups

3. trace file format
 - first line: length of trace, size of disk (both in number of pages)
 - after: offset (page number), length (page number), stream number