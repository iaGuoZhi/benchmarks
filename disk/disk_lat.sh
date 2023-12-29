#!/bin/bash

test_file=$(mktemp)
file_size="1G"

echo "Creating test file..."
fallocate -l "$file_size" "$test_file"

run_read_test() {
  echo "Running read test..."
  fio --name=read_test --rw=read --filename="$test_file" --direct=1 --ioengine=sync --bs=4k --size="$file_size" --numjobs=1 --time_based --runtime=10s --output-format=json --output=read_test_result.json > /dev/null
}

run_write_test() {
  echo "Running write test..."
  fio --name=write_test --rw=write --filename="$test_file" --direct=1 --ioengine=sync --bs=4k --size="$file_size" --numjobs=1 --time_based --runtime=10s --output-format=json --output=write_test_result.json > /dev/null
}

run_read_test

run_write_test

read_latency=$(jq -r '.jobs[0].read.clat_ns.percentile["99.900000"]' read_test_result.json)
echo "Read latency: $read_latency ns"

write_latency=$(jq -r '.jobs[0].write.clat_ns.percentile["99.900000"]' write_test_result.json)
echo "Write latency: $write_latency ns"

read_bandwidth=$(jq -r '.jobs[0].read.bw' read_test_result.json)
read_bandwidth_mb=$(echo "scale=2; $read_bandwidth / (1024*1024)" | bc)
echo "Read bandwidth: $read_bandwidth_mb MB/s"

write_bandwidth=$(jq -r '.jobs[0].write.bw' write_test_result.json)
write_bandwidth_mb=$(echo "scale=2; $write_bandwidth / (1024*1024)" | bc)
echo "Write bandwidth: $write_bandwidth_mb MB/s"

echo "Cleaning up..."
rm "$test_file" read_test_result.json write_test_result.json

echo "Test completed."

