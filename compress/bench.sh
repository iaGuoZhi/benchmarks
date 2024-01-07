#!/bin/bash

# make sure compress programs exist
#works=("./code/fpzip/build/bin/fpzip" "./code/pFPC/pfpc" "./code/SPDP/spdp" "./code/ndzip/build/compress" "./code/ndzip/build/compress" "./code/buff/database/target/release/comp_profiler" "./code/GFC/GFC")
works=("./code/fpzip/build/bin/fpzip" "./code/pFPC/pfpcb" "./code/SPDP/spdpb")
for work in ${works[@]}; do
  if [ ! -f $work ]; then
    echo "File '$work' does not exist. Please build it by build.sh before running $0"
    exit 1
  fi
done

files=("./data/msg_bt_f64" "./data/num_brain_f64" "./data/num_control_f64" "./data/turbulence_f32")
for file in ${files[@]}; do
  if [ ! -f $file ]; then
    echo "File '$file' does not exist. Please download it by download_dataset.sh before running $0"
    exit 1
  fi
done
dimensions=(1 1 1 3)
data_types=("double" "double" "double" "float")
shapes=('25896227' '15249383' '19222421' '256 256 256')

# make sure results directory exists
if [ ! -d "results" ]; then
  mkdir "results"
fi

# create a dir named as timestamp
dir=results/compress_$(date +"%Y%m%d%H%M%S")
mkdir -p $dir

out_dir=output
if [ ! -d "$out_dir" ]; then
  mkdir "$out_dir"
fi

# test fpzip
work=./code/fpzip/build/bin/fpzip
for i in "${!files[@]}"; do
  echo ${files[i]} | tr '\n' ' ' >> "$dir/fpzip.log"
  { $work -i ${files[i]} -t ${data_types[i]} -${dimensions[i]} ${shapes[i]} ;} 2>> "$dir/fpzip.log"
done
awk '{print $1 $5 $7 $10}' $dir/fpzip.log > $dir/fpzip.res

## compress ratio
awk '{array[NR] = $5} END {print "fpzip"; for (i=1; i<=NR; i++) print array[i]}' >> $dir/compress_ratio.csv
## compress time
awk '{array[NR] = $7} END {print "fpzip"; for (i=1; i<=NR; i++) print array[i]}' >> $dir/compress_time.csv
## decompress time
awk '{array[NR] = $10} END {print "fpzip"; for (i=1; i<=NR; i++) print array[i]}' >> $dir/decompress_time.csv


# test pFPC
work=./code/pFPC/pfpcb
SIZE=1048576
for i in "${!files[@]}"; do
  echo ${files[i]} | tr '\n' ' ' >> "$dir/pfpc_comp.log"
 	{ $work 20 8 2048 $SIZE < ${files[i]}  > $out_dir/compress.pfpc ;} 2>> "$dir/pfpc_comp.log"
  echo ${files[i]} | tr '\n' ' ' >> "$dir/pfpc_decomp.log"
 	{ $work $SIZE < $out_dir/compress.pfpc  > $out_dir/compress.pfpcout ;} 2>> "$dir/pfpc_decomp.log"
done

# test SPDP
work=./code/SPDP/spdpb
SIZE=1048576
for i in "${!files[@]}"; do
  echo ${files[i]} | tr '\n' ' ' >> "$dir/spdp_comp.log"
	{ $work 10 $SIZE < ${files[i]} > $out_dir/compress.spdp ;} 2>> "$dir/spdp_comp.log"
  echo ${files[i]} | tr '\n' ' ' >> "$dir/spdp_decomp.log"
  { $work $SIZE < $out_dir/compress.spdp  > $out_dir/compress.spdpout ;} 2>> "$dir/spdp_decomp.log"
done

rm -rf $out_dir
