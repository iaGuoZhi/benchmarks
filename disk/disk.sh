#!/bin/bash

if ! command -v fio >/dev/null 2>&1; then
  echo "fio havn't been installed. Please install it before running $0"
  exit 1
fi
if ! command -v jq >/dev/null 2>&1; then
  echo "jq havn't been installed. Please install it before running $0"
  exit 1
fi

function print_usage() {
  RED='\033[0;31m'
  BLUE='\033[0;34m'
  BOLD='\033[1m'
  NONE='\033[0m'

  echo -e "\n${RED}Usage${NONE}: ${BOLD}$0${NONE} [FILE_PATH] [OPTION]"

  echo -e "\n${RED}OPTIONS${NONE}:
  ${BLUE}throughput${NONE}    test sequential read/write bandwidth
  ${BLUE}concurrent${NONE}    test multiple concurrent sequentials read/write bandwidth
  ${BLUE}iops${NONE}    test random read/write iops
  ${BLUE}latency${NONE}   test read/write latency
  ${BLUE}all${NONE}   test all above target"

}

function calc_duration() {
  duration=$1
  hours=$((duration / 3600))
  minutes=$((duration % 3600 / 60))
  seconds=$((duration % 60))
  duration=""
  if [[ $hours -gt 0 ]]; then
    duration="$hours hours,"
  fi
  if [[ $minutes -gt 0 ]]; then
    duration="$duration $minutes minutes,"
  fi
  duration="$duration $seconds seconds"
}

function save_info() {
  echo -e "Command: $0 $option" >> $dir/info.txt
  echo -e "Time: $(date +"%Y-%m-%d %H:%M:%S")" >> $dir/info.txt
  echo -e "Duration: $duration" >> $dir/info.txt
  echo -e "Hostname: $(hostname)" >> $dir/info.txt
}

options=("throughput" "concurrent" "iops" "latency" "all")

if [[ $# -lt 1 ]]; then
  exit 1
fi
file_path=$1

if [[ $# -lt 2 ]]; then
  option="all"
else
  if [[ ! "${options[*]}" =~ "$2" ]]; then
    print_usage
    exit 1
  else
    option=$2
  fi
fi

echo "FILE_PATH: $file_path"
echo "OPTION: $option"

# make sure results directory exists
if [ ! -d "results" ]; then
  mkdir "results"
fi

# create a dir named as timestamp
dir=results/disk_$(hostname)_${option}_$(date +"%Y%m%d%H%M%S")
mkdir -p $dir

file_size="16m"

if [[ $option == "throughput" ||  $option == "all" ]]; then
  csv_header="block_size,read(mb/s),write(mb/s)"
  block_sizes=("16k" "32k" "64k" "128k" "256k" "512k" "1m" "2m" "4m" "8m")
	echo $csv_header >> $dir/disk_thr.csv
  start_time=$(date +%s)
  for block_size in ${block_sizes[@]};
  do
    /bin/bash ./disk_thr.sh $file_size $block_size $file_path | awk '{print $8 "," $11 "," $15}' >> $dir/disk_thr.csv
  done
  end_time=$(date +%s)
  calc_duration $((end_time - start_time))
  echo "Measurement duration: $duration"
  save_info
  python3 ./disk_thr.py $dir
  echo "$option image are saved in $dir/disk_thr.png"
fi

if [[ $option == "concurrent" ||  $option == "all" ]]; then
  csv_header="block_size,read(mb/s),write(mb/s)"
  block_sizes=("4k" "8k" "16k" "32k" "64k" "128k" "256k" "512k" "1m" "2m")
	echo $csv_header >> $dir/disk_conc.csv
  start_time=$(date +%s)
  for block_size in ${block_sizes[@]};
  do
    /bin/bash ./disk_conc.sh $file_size $block_size $file_path | awk '{print $8 "," $11 "," $15}' >> $dir/disk_conc.csv
  done
  end_time=$(date +%s)
  calc_duration $((end_time - start_time))
  echo "Measurement duration: $duration"
  save_info
  python3 ./disk_conc.py $dir
  echo "$option image are saved in $dir/disk_conc.png"
fi

if [[ $option == "iops" ||  $option == "all" ]]; then
  csv_header="block_size,read,write"
  block_sizes=("512" "1k" "2k" "4k" "8k" "16k" "32k" "64k" "128k" "256k")
	echo $csv_header >> $dir/disk_iops.csv
  start_time=$(date +%s)
  for block_size in ${block_sizes[@]};
  do
    /bin/bash ./disk_iops.sh $file_size $block_size $file_path | awk '{print $8 "," $11 "," $14}' >> $dir/disk_iops.csv
  done
  end_time=$(date +%s)
  calc_duration $((end_time - start_time))
  echo "Measurement duration: $duration"
  save_info
  python3 ./disk_iops.py $dir
  echo "$option image are saved in $dir/disk_iops.png"
fi

if [[ $option == "latency" ||  $option == "all" ]]; then
  csv_header="block_size,read(ns),write(ns)"
  block_sizes=("512" "1k" "2k" "4k" "8k" "16k" "32k" "64k" "128k" "256k")
	echo $csv_header >> $dir/disk_lat.csv
  start_time=$(date +%s)
  for block_size in ${block_sizes[@]};
  do
    /bin/bash ./disk_lat.sh $file_size $block_size $file_path | awk '{print $8 "," $11 "," $15}' >> $dir/disk_lat.csv
  done
  end_time=$(date +%s)
  calc_duration $((end_time - start_time))
  echo "Measurement duration: $duration"
  save_info
  python3 ./disk_lat.py $dir
  echo "$option image are saved in $dir/disk_lat.png"
fi
