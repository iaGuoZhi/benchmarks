#!/bin/bash

test_file=$(mktemp)
file_size="1G"

echo "Creating test file..."
fallocate -l "$file_size" "$test_file"

run_random_read_test() {
  echo "Running random read test..."
  fio --name=random_read_test --rw=randread --filename="$test_file" --direct=1 --ioengine=sync --bs=4k --size="$file_size" --numjobs=1 --time_based --runtime=10s --output-format=json --output=random_read_test_result.json > /dev/null
}

run_random_write_test() {
  echo "Running random write test..."
  fio --name=random_write_test --rw=randwrite --filename="$test_file" --direct=1 --ioengine=sync --bs=4k --size="$file_size" --numjobs=1 --time_based --runtime=10s --output-format=json --output=random_write_test_result.json > /dev/null
}

run_random_read_test

run_random_write_test

random_read_iops=$(jq -r '.jobs[0].read.iops' random_read_test_result.json)
echo "Random read IOPS: $random_read_iops"

random_write_iops=$(jq -r '.jobs[0].write.iops' random_write_test_result.json)
echo "Random write IOPS: $random_write_iops"

echo "Cleaning up..."
rm "$test_file" random_read_test_result.json random_write_test_result.json

echo "Test completed."

