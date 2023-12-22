#!/bin/bash

work=./mem_lat

# l1 data cache size 32kb per core, 8 way
# l2 cache size 256kb per core, 4 way
# l3 cache size 9 * 1024 kb, 12 way

# buffer size(kb)
buffer_size_begin=16
buffer_size_end=10240

# stride choices
# stride=64, default cache line size
# stride=256, default cache line size * 4(l2 cache is 4 way associate)
strides=(64 128 256 512)

# create a dir named as timestamp
dir=results/mem_lat_$(date +"%Y%m%d%H%M%S")
mkdir -p $dir

for stride in ${strides[@]}
do
  for buffer_size in $(seq $buffer_size_begin 4 $buffer_size_end); do
      taskset -ac 2 $work -b $buffer_size -s $stride
  done > $dir/mem_lat_$stride.log
done

paste -d' ' $dir/mem_lat_64.log $dir/mem_lat_128.log $dir/mem_lat_256.log $dir/mem_lat_512.log > $dir/mem_lat.log
cat $dir/mem_lat.log | awk 'BEGIN{print "buffer_size,stride 64,stride 128,stride 256,stride 512"} {print $3 "," $11 "," $23 "," $35 "," $47}' > $dir/mem_lat.csv

