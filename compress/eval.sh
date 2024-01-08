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
file_size=(266389432 141840000 159504744 67108864)
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
sed 's/\=/\ /g' $dir/fpzip.log | awk '{print $1 " " $5 " " $7 " " $10}' > $dir/fpzip.res

# test pFPC
work=./code/pFPC/pfpcb
SIZE=1048576
for i in "${!files[@]}"; do
  echo ${files[i]} | tr '\n' ' ' >> "$dir/pfpcb_comp.log"
 	{ $work 20 8 2048 $SIZE < ${files[i]}  > $out_dir/compress.pfpcb ;} 2>> "$dir/pfpcb_comp.log"
  echo ${files[i]} | tr '\n' ' ' >> "$dir/pfpcb_decomp.log"
 	{ $work $SIZE < $out_dir/compress.pfpcb  > $out_dir/compress.pfpcbout ;} 2>> "$dir/pfpcb_decomp.log"
done
awk '{ print $1 " " $3 " " $8}' $dir/pfpcb_comp.log > $dir/pfpcb_comp.res
awk '{ print $6 }' $dir/pfpcb_decomp.log > $dir/pfpcb_decomp.res
paste -d ' ' $dir/pfpcb_comp.res $dir/pfpcb_decomp.res > $dir/pfpcb.res

## test SPDP
work=./code/SPDP/spdpb
SIZE=1048576
for i in "${!files[@]}"; do
  echo ${files[i]} | tr '\n' ' ' >> "$dir/spdpb_comp.log"
	{ $work 10 $SIZE < ${files[i]} > $out_dir/compress.spdpb ;} 2>> "$dir/spdpb_comp.log"
  echo ${files[i]} | tr '\n' ' ' >> "$dir/spdpb_decomp.log"
  { $work $SIZE < $out_dir/compress.spdpb  > $out_dir/compress.spdpbout ;} 2>> "$dir/spdpb_decomp.log"
done
awk '{ print $1 " " $3 " " $8}' $dir/spdpb_comp.log > $dir/spdpb_comp.res
awk '{ print $6 }' $dir/spdpb_decomp.log > $dir/spdpb_decomp.res
paste -d ' ' $dir/spdpb_comp.res $dir/spdpb_decomp.res > $dir/spdpb.res

# prepare csv header
tmp_file=$(mktemp)
for i in "${!files[@]}"; do
  if [ $i -eq 0 ]; then
    printf "work" > $tmp_file
  fi
  printf ",%s" $(basename ${files[i]}) >> $tmp_file
done

# compress ratio
cp $tmp_file $dir/compress_ratio.csv
for work in ${works[@]}; do
  label=$(basename $work)
  awk -v label="$label" '{array[NR] = $2} \
  END {printf "\n%s", label; for (i=1; i<=NR; i++) { printf ",%s", array[i] }}' \
  $dir/${label}.res >> $dir/compress_ratio.csv
done

# compress throughput
cp $tmp_file $dir/compress_throughput.csv
for work in ${works[@]}; do
  label=$(basename $work)
  awk -v label="$label" -v file_size="${file_size[*]}" 'BEGIN {split(file_size, size, " ")} \
    {array[NR] = $3} \
    END {printf "\n%s", label; for (i=1; i<=NR; i++) \
    { printf ",%.2f", size[i] * 1000 / 1024 / 1024 / 1024 / strtonum(array[i]) }}'\
    $dir/${label}.res >> $dir/compress_throughput.csv
done

# decompress throughput
cp $tmp_file $dir/decompress_throughput.csv
for work in ${works[@]}; do
  label=$(basename $work)
  awk -v label="$label" -v file_size="${file_size[*]}" 'BEGIN {split(file_size, size, " ")} \
    {array[NR] = $4} \
    END {printf "\n%s", label; for (i=1; i<=NR; i++) \
    { printf ",%.2f", size[i] * 1000 / 1024 / 1024 / 1024 / strtonum(array[i]) }}'\
    $dir/${label}.res >> $dir/decompress_throughput.csv
done

# draw
python3 ./draw.py $dir

rm -rf $out_dir
