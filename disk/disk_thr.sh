#!/bin/bash

file_size=$1
block_size=$2
test_file=$3

fallocate -l "$file_size" "$test_file"

run_read_test() {
  fio --name=read_test --rw=read --filename="$test_file" --direct=1 --ioengine=sync --bs=$block_size --size=$file_size --numjobs=1 --time_based --runtime=10s --output-format=json --output=read_test_result.json > /dev/null
}

run_write_test() {
  fio --name=write_test --rw=write --filename="$test_file" --direct=1 --ioengine=sync --bs=$block_size --size=$file_size --numjobs=1 --time_based --runtime=10s --output-format=json --output=write_test_result.json > /dev/null
}

run_read_test

run_write_test

read_bandwidth=$(jq -r '.jobs[0].read.bw' read_test_result.json)
read_bandwidth_mb=$(echo "scale=2; $read_bandwidth / (1024*1024)" | bc)

write_bandwidth=$(jq -r '.jobs[0].write.bw' write_test_result.json)
write_bandwidth_mb=$(echo "scale=2; $write_bandwidth / (1024*1024)" | bc)

echo "File: $test_file file size: $file_size block size: $block_size read throughput: $read_bandwidth_mb MB/s write throughput: $write_bandwidth_mb MB/s"
rm "$test_file" read_test_result.json write_test_result.json
