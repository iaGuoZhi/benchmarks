#!/bin/bash

work=./ram_lat
if [ ! -f "$work" ]; then
  echo "File '$work' does not exist. Please compile it before running $0"
  exit 1
fi

function print_usage() {
	RED='\033[0;31m'
	BLUE='\033[0;34m'
	BOLD='\033[1m'
	NONE='\033[0m'

  # Looks ungly because we can't get tREFI from OS
  echo -e "\n${RED}Usage${NONE}: ${BOLD}$0${NONE} [tREFI]"

  echo -e "\n${RED}tREFI${NONE}:
  ${BLUE}0${NONE}    tREFI unknown
  ${BLUE}number${NONE}    number of tREFI in BIOS
  "
}

function save_info() {
  echo -e "Time: $(date +"%Y-%m-%d %H:%M:%S")" >> $dir/info.txt
  echo -e "Hostname: $(hostname)" >> $dir/info.txt
  if [[ $tREFI -eq 0 ]]; then
    echo "tREFI: unknown" >> $dir/info.txt
  else
    echo "tREFI: $tREFI" >> $dir/info.txt
  fi
  echo -e "CPU model:
  $(cat /proc/cpuinfo | grep "model name" | uniq | cut -d':' -f2-)" >> $dir/info.txt
  echo "CPU Frequency(kHZ): $cpu_freq" >> $dir/info.txt
  echo "Memory Frequency(MT/s): $mem_freq" >> $dir/info.txt
  echo "Frequency spike: $frequency_spike" >> $dir/info.txt
  echo "Interval(ns): $interval" >> $dir/info.txt
}

if [[ $# -eq 0 ]]; then
  print_usage
  exit 1
fi

tREFI=$1

# make sure results directory exists
if [ ! -d "results" ]; then
  mkdir "results"
fi
# create a dir named as timestamp
dir=results/ram_lat_$(date +"%Y%m%d%H%M%S")
mkdir -p $dir

taskset -ac 2 $work > $dir/ram_lat.csv
echo "Result are saved in $dir/ram_lat.csv"

# collect hardware info
cpu_freq_file=/sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq
if [ -f "$cpu_freq_file" ]; then
    cpu_freq=$(cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq)
else
    cpu_freq="Unknown"
fi
if sudo -n -v >/dev/null 2>&1; then
    mem_freq=$(sudo dmidecode -t memory | grep "Speed" | head -n 1 | awk '{print $2}')
else
    mem_freq="Unknown"
fi

# run multiple times to get minimal frequency spike, which can help to avoid the effect of harmonics
echo "Run multiple times to avoid harmonics"
results=()
for ((i=1; i<=10; i++))
do
    output=$($work | python3 ./ram_lat_analyze.py)
    last_line=$(echo "$output" | tail -n 1)
    results+=("$last_line")
done

# get minimal Frequency spike
min_value=${results[0]}
for result in "${results[@]}"
do
    if (( $(echo "$result < $min_value" | bc -l) ))
    then
        min_value=$result
    fi
done

frequency_spike=$(printf "%.2f" $min_value)
interval=$(echo "1000000000/$min_value" | bc)
echo "Frequency spike: $frequency_spike"
echo "Interval(ns): $interval"

save_info

# make sure python packages in requirements.txt have been installed
if [ -f "requirements.txt" ]; then
  while read -r package; do
    if python -c "import $package" >/dev/null 2>&1; then
      echo "Installing $package..."
      pip install "$package"
    fi
  done < requirements.txt > /dev/null 2>&1
fi

python3 ./ram_lat_draw.py $dir
echo "Image are saved in $dir/ram_lat.png"
