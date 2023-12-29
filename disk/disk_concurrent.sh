#!/bin/bash

test_file=$(mktemp)
file_size="1G"

# 创建测试文件
echo "Creating test file..."
fallocate -l "$file_size" "$test_file"

# 定义并发顺序读取测试函数
run_concurrent_sequential_read_test() {
  num_jobs=$1
  echo "Running concurrent sequential read test with $num_jobs jobs..."
  fio --name=concurrent_sequential_read_test --rw=read --filename="$test_file" --direct=1 --ioengine=sync --bs=1M --size="$file_size" --numjobs="$num_jobs" --time_based --runtime=10s --output-format=json --output=concurrent_sequential_read_test_result.json > /dev/null
}

# 定义并发顺序写入测试函数
run_concurrent_sequential_write_test() {
  num_jobs=$1
  echo "Running concurrent sequential write test with $num_jobs jobs..."
  fio --name=concurrent_sequential_write_test --rw=write --filename="$test_file" --direct=1 --ioengine=sync --bs=1M --size="$file_size" --numjobs="$num_jobs" --time_based --runtime=10s --output-format=json --output=concurrent_sequential_write_test_result.json > /dev/null
}

# 执行并发顺序读取测试
run_concurrent_sequential_read_test 4

# 执行并发顺序写入测试
run_concurrent_sequential_write_test 4

concurrent_sequential_read_bw_sum=0
for bw in $(jq -r '.jobs[].read.bw' concurrent_sequential_read_test_result.json); do
  concurrent_sequential_read_bw_sum=$(echo "$concurrent_sequential_read_bw_sum + $bw" | bc)
done
concurrent_sequential_read_bw_avg=$(echo "scale=2; $concurrent_sequential_read_bw_sum / 4 / 1024 / 1024" | bc)
echo "Concurrent sequential read average bandwidth: $concurrent_sequential_read_bw_avg MB/s"

# 提取并发顺序写入测试结果并计算平均带宽
concurrent_sequential_write_bw_sum=0
for bw in $(jq -r '.jobs[].write.bw' concurrent_sequential_write_test_result.json); do
  concurrent_sequential_write_bw_sum=$(echo "$concurrent_sequential_write_bw_sum + $bw" | bc)
done
concurrent_sequential_write_bw_avg=$(echo "scale=2; $concurrent_sequential_write_bw_sum / 4 / 1024 / 1024 " | bc)
echo "Concurrent sequential write average bandwidth: $concurrent_sequential_write_bw_avg MB/s"

# 清理测试文件和结果文件
echo "Cleaning up..."
rm "$test_file" concurrent_sequential_read_test_result.json concurrent_sequential_write_test_result.json

echo "Test completed."

