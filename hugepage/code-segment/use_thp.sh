
cd ./iodlr/large_page-c
make -f Makefile.preload
cd ../../

perf stat -e iTLB-loads,iTLB-load-misses env LD_PRELOAD=./iodlr/large_page-c/liblppreload.so ./large_elf
