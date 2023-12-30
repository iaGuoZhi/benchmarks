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

read_latency=$(jq -r '.jobs[0].read.clat_ns.percentile["99.000000"]' read_test_result.json)

write_latency=$(jq -r '.jobs[0].write.clat_ns.percentile["99.000000"]' write_test_result.json)

echo "File: $test_file file size: $file_size block size: $block_size read latency: $read_latency ns write latency: $write_latency ns"

rm "$test_file" read_test_result.json write_test_result.json
