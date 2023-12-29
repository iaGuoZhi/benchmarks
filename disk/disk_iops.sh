#!/bin/bash

test_file=$(mktemp)
file_size=$1
block_size=$2

fallocate -l "$file_size" "$test_file"

run_random_read_test() {
  fio --name=random_read_test --rw=randread --filename="$test_file" --direct=1 --ioengine=sync --bs=$block_size --size="$file_size" --numjobs=1 --time_based --runtime=10s --output-format=json --output=random_read_test_result.json > /dev/null
}

run_random_write_test() {
  fio --name=random_write_test --rw=randwrite --filename="$test_file" --direct=1 --ioengine=sync --bs=$block_size --size="$file_size" --numjobs=1 --time_based --runtime=10s --output-format=json --output=random_write_test_result.json > /dev/null
}

run_random_read_test

run_random_write_test

random_read_iops=$(jq -r '.jobs[0].read.iops' random_read_test_result.json)

random_write_iops=$(jq -r '.jobs[0].write.iops' random_write_test_result.json)
echo "File: $test_file file size: $file_size block size: $block_size read iops: $random_read_iops write iops: $random_write_iops"

rm "$test_file" random_read_test_result.json random_write_test_result.json
