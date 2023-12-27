#!/bin/bash

work=./cache_lat
if [ ! -f "$work" ]; then
  echo "File '$work' does not exist. Please compile it before running $0"
  exit 1
fi

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
  ${BLUE}all${NONE}   test all cache latency(default)"

  echo -e "\n${RED}FULL/FAST${NONE}:
  ${BLUE}full${NONE}   test with small interval and long time
  ${BLUE}fast${NONE}   test with large interval and short time(default)
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

options=("l1" "l2" "l3" "all")
modes=("fast" "full")

if [[ $# -lt 1 ]]; then
  option="all"
else
  if [[ ! "${options[*]}" =~ "$1" ]]; then
    print_usage
    exit 1
  else
    option=$1
  fi
fi

if [[ $# -lt 2 ]]; then
  mode="fast"
else
  if [[ ! "${modes[*]}" =~ "$2" ]]; then
    print_usage
    exit 1
  else
    mode=$2
  fi
fi

echo "OPTION: $option"
echo "MODE: $mode"

# make sure results directory exists
if [ ! -d "results" ]; then
  mkdir "results"
fi
# create a dir named as timestamp
dir=results/cache_lat_$(hostname)_${option}_${mode}_$(date +"%Y%m%d%H%M%S")
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

l1_range_begin=$((l1_cache_size/2))
l1_range_end=$((l1_cache_size+l1_cache_size/2))
l2_range_begin=$((l2_cache_size/2))
l2_range_end=$((l2_cache_size+l2_cache_size/2))
l3_range_begin=$((l3_cache_size/2))
l3_range_end=$((l3_cache_size+l3_cache_size/2))
# make sure l1_range_end is not larger than l2_range_begin, so as l2_range_end and l3_range_begin.
if [[ $l1_range_end -gt $l2_range_begin ]]; then
  l1_range_end=$l2_range_begin
fi
if [[ $l2_range_end -gt $l3_range_begin ]]; then
  l2_range_end=$l3_range_begin
fi

# measure cache latency by range, different range has different interval
range_begins=($l1_range_begin $l1_range_end $l2_range_begin $l2_range_end $l3_range_begin)
range_ends=($l1_range_end $l2_range_begin $l2_range_end $l3_range_begin $l3_range_end)
# which option measure the range
range_options=("l1 all" "all" "l2 all" "all" "l3 all")
full_mode_range_intervals=(2 4 4 8 8)
fast_mode_range_intervals=(4 8 16 32 64)
# stride choices, default cache line size is 64 bytes
strides=(64 128 256 512)

if [[ $mode == *"full"* ]]; then
  range_intervals=(${full_mode_range_intervals[@]})
else
  range_intervals=(${fast_mode_range_intervals[@]})
fi

# measure cache latency by running $work
start_time=$(date +%s)
for stride in ${strides[@]};
do
  for ((i = 0; i < ${#range_begins[@]}; i++)); do
    range_begin=${range_begins[$i]}
    range_end=${range_ends[$i]}
    range_option=${range_options[$i]}
    range_interval=${range_intervals[$i]}
    if grep -qF "$option" <<< "$range_option"; then
      temp_file=$(mktemp)
      for buffer_size in $(seq $range_begin $interval $range_end); do
        # set core affinity
        taskset -ac 2 $work -b $buffer_size -s $stride
      done > $temp_file
      if [[ $option == "all" ]]; then
        # redirect output to multiple files
        range_option=($range_option)
        for op in ${range_option[@]};
        do
          cat $temp_file >> $dir/cache_lat_${op}_$stride.log
        done
      else
        mv $temp_file $dir/cache_lat_${option}_$stride.log
	  fi
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

# generate csv file for each available option
for op in ${options[@]}; do
  if [ -n "$(find "$dir" -type f -name "cache_lat_${op}*" -print -quit 2>/dev/null)" ]; then
    # merge all log files
    for stride in ${strides[@]}; do
      echo "$dir/cache_lat_${op}_$stride.log"
    done | xargs paste -d' ' > $dir/cache_lat_$op.log

    # convert to csv format
    cat $dir/cache_lat_$op.log | awk -v csv_header="$csv_header" -v csv_cols="$csv_cols" \
      'BEGIN{print csv_header; FS=" "; OFS=","; split(csv_cols, cols, " ");} \
      {for (i in cols) {printf "%s%s", $cols[i], (i==length(cols)?"\n":OFS);}}' \
    > $dir/cache_lat_$op.csv
    #paste -d' ' $dir/cache_lat_${op}_64.log $dir/cache_lat_${op}_128.log $dir/cache_lat_${op}_256.log $dir/cache_lat_${op}_512.log > $dir/cache_lat_${op}.log
    #cat $dir/cache_lat_$op.log | awk 'BEGIN{print "buffer_size,stride 64,stride 128,stride 256,stride 512"} {print $3 "," $11 "," $23 "," $35 "," $47}' > $dir/cache_lat_$op.csv

    echo "$op cache results are saved in $dir/cache_lat_$op.csv"
  fi
done

# make sure python packages in requirements.txt have been installed
if [ -f "requirements.txt" ]; then
  while read -r package; do
    if python -c "import $package" >/dev/null 2>&1; then
      echo "Installing $package..."
      pip install "$package"
    fi
  done < requirements.txt
fi

# draw the graph
for op in ${options[@]}; do
  if [ -n "$(find "$dir" -type f -name "cache_lat_${op}*" -print -quit 2>/dev/null)" ]; then
    python3 ./cache_lat.py $dir $op
    echo "$op cache image are saved in $dir/cache_lat_$op.png"
  fi
done
