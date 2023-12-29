#!/bin/bash

test_file=$(mktemp)
file_size=$1
block_size=$2

fallocate -l "$file_size" "$test_file"

run_concurrent_sequential_read_test() {
  num_jobs=$1
  fio --name=concurrent_sequential_read_test --rw=read --filename="$test_file" --direct=1 --ioengine=sync --bs=$block_size --size="$file_size" --numjobs="$num_jobs" --time_based --runtime=10s --output-format=json --output=concurrent_sequential_read_test_result.json > /dev/null
}

run_concurrent_sequential_write_test() {
  num_jobs=$1
  fio --name=concurrent_sequential_write_test --rw=write --filename="$test_file" --direct=1 --ioengine=sync --bs=$block_size --size="$file_size" --numjobs="$num_jobs" --time_based --runtime=10s --output-format=json --output=concurrent_sequential_write_test_result.json > /dev/null
}

run_concurrent_sequential_read_test 4

run_concurrent_sequential_write_test 4

concurrent_sequential_read_bw_sum=0
for bw in $(jq -r '.jobs[].read.bw' concurrent_sequential_read_test_result.json); do
  concurrent_sequential_read_bw_sum=$(echo "$concurrent_sequential_read_bw_sum + $bw" | bc)
done
concurrent_sequential_read_bw_avg=$(echo "scale=2; $concurrent_sequential_read_bw_sum / 4 / 1024 / 1024" | bc)

concurrent_sequential_write_bw_sum=0
for bw in $(jq -r '.jobs[].write.bw' concurrent_sequential_write_test_result.json); do
  concurrent_sequential_write_bw_sum=$(echo "$concurrent_sequential_write_bw_sum + $bw" | bc)
done
concurrent_sequential_write_bw_avg=$(echo "scale=2; $concurrent_sequential_write_bw_sum / 4 / 1024 / 1024 " | bc)

echo "File: $test_file file size: $file_size block size: $block_size read throughput: $concurrent_sequential_read_bw_avg MB/s write throughput: $concurrent_sequential_write_bw_avg MB/s"

rm "$test_file" concurrent_sequential_read_test_result.json concurrent_sequential_write_test_result.json
