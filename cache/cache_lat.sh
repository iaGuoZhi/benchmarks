#!/bin/bash

work=./cache_lat

function print_usage() {
	RED='\033[0;31m'
	BLUE='\033[0;34m'
	BOLD='\033[1m'
	NONE='\033[0m'

  echo -e "\n${RED}Usage${NONE}: ${BOLD}$0${NONE} [OPTION] [FULL/FAST]"

  echo -e "\n${RED}OPTIONS${NONE}:
  ${BLUE}l1${NONE}    test l1 cache latency
  ${BLUE}l2${NONE}    test l2 cache latency
  ${BLUE}l3${NONE}    test l3 cache latency
  ${BLUE}all${NONE}   test all cache latency"

  echo -e "\n${RED}FULL/FAST${NONE}:
  ${BLUE}full${NONE}   test with small interval and long time
  ${BLUE}fast${NONE}   test with large interval and short time
  "
}

function print_cache_info() {
	RED='\033[0;31m'
	BLUE='\033[0;34m'
	BOLD='\033[1m'
	NONE='\033[0m'

  echo -e "${RED}Cache Info${NONE}:
  ${BLUE}l1 cache size${NONE}    $l1_cache_size KB
  ${BLUE}l1 cache ways${NONE}    $l1_cache_ways
  ${BLUE}l2 cache size${NONE}    $l2_cache_size KB
  ${BLUE}l2 cache ways${NONE}    $l2_cache_ways
  ${BLUE}l3 cache size${NONE}    $l3_cache_size KB
  ${BLUE}l3 cache ways${NONE}    $l3_cache_ways
  "
}

function save_info() {
  echo -e "Command: $0 $option $mode" >> $dir/info.txt
  echo -e "Time: $(date +"%Y-%m-%d %H:%M:%S")" >> $dir/info.txt
  echo -e "Duration: $duration" >> $dir/info.txt
  echo -e "Hostname: $(hostname)" >> $dir/info.txt
  echo -e "CPU model:
  $(cat /proc/cpuinfo | grep "model name" | uniq | cut -d':' -f2-)" >> $dir/info.txt
  echo -e "Cache Info:
  l1 cache size    $l1_cache_size KB
  l1 cache ways    $l1_cache_ways
  l2 cache size    $l2_cache_size KB
  l2 cache ways    $l2_cache_ways
  l3 cache size    $l3_cache_size KB
  l3 cache ways    $l3_cache_ways
  " >> $dir/info.txt
}

if [[ $1 != "l1" && $1 != "l2" && $1 != "l3" && $1 != "all" ]]; then
  print_usage
  exit 1
fi
option=$1

if [[ $2 != *"full"* && $2 != *"fast"* ]]; then
  print_usage
  exit 1
fi
mode=$2

# create a dir named as timestamp
dir=results/cache_lat_$(date +"%Y%m%d%H%M%S")
mkdir -p $dir

# collect cache information(cache size is in KB)
l1_cache_size=$(cat /sys/devices/system/cpu/cpu0/cache/index0/size)
l1_cache_size=${l1_cache_size%K}
l1_cache_size=$((l1_cache_size))
l1_cache_ways=$(cat /sys/devices/system/cpu/cpu0/cache/index0/ways_of_associativity)
l2_cache_size=$(cat /sys/devices/system/cpu/cpu0/cache/index2/size)
l2_cache_size=${l2_cache_size%K}
l2_cache_size=$((l2_cache_size))
l2_cache_ways=$(cat /sys/devices/system/cpu/cpu0/cache/index2/ways_of_associativity)
l3_cache_size=$(cat /sys/devices/system/cpu/cpu0/cache/index3/size)
l3_cache_size=${l3_cache_size%K}
l3_cache_size=$((l3_cache_size))
l3_cache_ways=$(cat /sys/devices/system/cpu/cpu0/cache/index3/ways_of_associativity)
print_cache_info

l1_buffer_size_begin=$((l1_cache_size/2))
l1_buffer_size_end=$((l1_cache_size+l1_cache_size/2))
l2_buffer_size_begin=$((l2_cache_size/2))
l2_buffer_size_end=$((l2_cache_size+l2_cache_size/2))
l3_buffer_size_begin=$((l3_cache_size/2))
l3_buffer_size_end=$((l3_cache_size+l3_cache_size/2))
# make sure l1_buffer_size_end is not larger than l2_buffer_size_begin, so as l2_buffer_size_end and l3_buffer_size_begin.
if [[ $l1_buffer_size_end -gt $l2_buffer_size_begin ]]; then
  l1_buffer_size_end=$l2_buffer_size_begin
fi
if [[ $l2_buffer_size_end -gt $l3_buffer_size_begin ]]; then
  l2_buffer_size_end=$l3_buffer_size_begin
fi

# measure cache latency by range, different range has different interval
buffer_size_range_begins=($l1_buffer_size_begin $l1_buffer_size_end $l2_buffer_size_begin $l2_buffer_size_end $l3_buffer_size_begin)
buffer_size_range_ends=($l1_buffer_size_end $l2_buffer_size_begin $l2_buffer_size_end $l3_buffer_size_begin $l3_buffer_size_end)
buffer_size_range_options=("l1 all" "all" "l2 all" "all" "l3 all")
full_mode_intervals=(2 4 4 8 8)
fast_mode_intervals=(2 8 16 64 128)
# stride choices, default cache line size is 64 bytes
strides=(64 128 256 512)

if [[ $mode == *"full"* ]]; then
  intervals=(${full_mode_intervals[@]})
else
  intervals=(${fast_mode_intervals[@]})
fi

# measure cache latency by running $work
start_time=$(date +%s)
for stride in ${strides[@]};
do
  for ((i = 0; i < ${#buffer_size_range_begins[@]}; i++)); do
    buffer_size_begin=${buffer_size_range_begins[$i]}
    buffer_size_end=${buffer_size_range_ends[$i]}
    options=${buffer_size_range_options[$i]}
    interval=${intervals[$i]}
    if grep -q $option <<< $options; then
      for buffer_size in $(seq $buffer_size_begin $interval $buffer_size_end); do
        # set core affinity
        taskset -ac 2 $work -b $buffer_size -s $stride
      done >> $dir/cache_lat_$stride.log
    fi
  done
  echo "Stride $stride finished"
done

# measure test duration
end_time=$(date +%s)
duration=$((end_time - start_time))
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
echo "Measurement duration: $duration"
save_info

# merge all log files
for stride in ${strides[@]}; do
  echo "$dir/cache_lat_$stride.log"
done | xargs paste -d' ' > $dir/cache_lat.log

# prepare csv header
csv_header="buffer_size"
for stride in ${strides[@]};
do
  csv_header="$csv_header,stride $stride"
done

# calculate the column number of each stride
csv_cols="3"
for i in $(seq 1 ${#strides[@]})
do
  col=$((i*12-1))
  csv_cols="$csv_cols $col"
done

# convert to csv format
cat $dir/cache_lat.log | awk -v csv_header="$csv_header" -v csv_cols="$csv_cols" \
  'BEGIN{print csv_header; FS=" "; OFS=","; split(csv_cols, cols, " ");} \
  {for (i in cols) {printf "%s%s", $cols[i], (i==length(cols)?"\n":OFS);}}' \
> $dir/cache_lat.csv

echo "Results are saved in $dir/cache_lat.csv"

# draw the graph
python3 ./cache_lat.py $dir
echo "Image are saved in $dir/cache_lat.png"
