#!/bin/bash

# make sure compress programs exist
#works=("../code/fpzip/build/bin/fpzip" "../code/pFPC/pfpc" "../code/SPDP/spdp" "../code/ndzip/build/compress" "../code/ndzip/build/compress" "../code/buff/database/target/release/comp_profiler" "../code/GFC/GFC")
#work_labels=("fpzip" "pFPC" "SPDP" "ndzip-cpu" "ndzip-gpu" "buff" "GFC")
works=("../code/fpzip/build/bin/fpzip" "../code/pFPC/pfpc" "../code/SPDP/spdp")
work_labels=("fpzip" "pFPC" "SPDP")
for work in ${works[@]}; do
  if [ ! -f $work ]; then
    echo "File '$dataset' does not exist. Please build it by build.sh before running $0"
    exit 1
  fi
done

datasets_double_precision=("../data/msg_bt_f64" "../data/num_brain_f64" "../data/num_control_f64")
datasets_single_precision=("../data/turbulence_f32")
datasets=("${datasets_double_precision[@]}" "${datasets_single_precision[@]}")
for dataset in ${datasets[@]}; do
  if [ ! -f $dataset ]; then
    echo "File '$dataset' does not exist. Please download it by download_dataset.sh before running $0"
    exit 1
  fi
done

# make sure results directory exists
if [ ! -d "results" ]; then
  mkdir "results"
fi

# create a dir named as timestamp
dir=results/compress_$(date +"%Y%m%d%H%M%S")
mkdir -p $dir

# test fpzip
work=../code/fpzip/build/bin/fpzip
for dataset in ${datasets_double_precision[@]}; do
  for i in {1..10}; do
    $work -i $dataset -t double -1 1024 2>> "$dir/$(basename $dataset).log"
  done
done

for dataset in ${datasets_single_precision[@]}; do
  for i in {1..10}; do
    $work -i $dataset -t float -1 1024 2>> "$dir/$(basename $dataset).log"
  done
done

# test pFPC
work=../code/pFPC/pfpc
for dataset in ${datasets_double_precision[@]}; do
  for i in {1..10}; do
    $work 20 8 2048 < $dataset -o > /dev/null 2>> "$dir/$(basename $dataset).log"
  done
done

for dataset in ${datasets_single_precision[@]}; do
  for i in {1..10}; do
    $work 20 8 1024 < $dataset -o > /dev/null 2>> "$dir/$(basename $dataset).log"
  done
done

# test SPDP
work=../code/SPDP/spdp
for dataset in ${datasets_double_precision[@]}; do
  for i in {1..10}; do
    $work 10 < $dataset > /dev/null 2>> "$dir/$(basename $dataset).log"
  done
done

for dataset in ${datasets_single_precision[@]}; do
  for i in {1..10}; do
    $work 10 < $dataset > /dev/null 2>> "$dir/$(basename $dataset).log"
  done
done
