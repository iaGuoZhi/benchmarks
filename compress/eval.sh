#!/bin/bash

# make sure compress programs exist
works=("./code/influxdb/tsdb/engine/tsm1/compress_test.go" "./code/influxdb/tsdb/engine/tsm1/compress_test.go")
labels=("gorilla" "chimp")
for work in ${works[@]}; do
  if [ ! -f $work ]; then
    echo "File '$work' does not exist. Please build it by build.sh before running $0"
    exit 1
  fi
done

files=("./data/air-sensor.csv" "./data/bird-migration.csv" "./data/bitcoin-historical.csv")
file_size=(340965 527784 247773)
for file in ${files[@]}; do
  if [ ! -f $file ]; then
    echo "File '$file' does not exist. Please download it by download_dataset.sh before running $0"
    exit 1
  fi
done

#dimensions=(1 1 1 1)
#data_types=("double" "double" "double" "double")

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

# test gorilla
for i in "${!files[@]}"; do
  echo ${files[i]} | tr '\n' ' ' >> "$dir/gorilla.log"
  yes | cp ${files[i]} code/influxdb/datasets/float.csv
  output_path=$(realpath $dir/gorilla.log)
  cd code/influxdb
  git checkout gorilla &> /dev/null
  go test -test.timeout 0 -run TestCompress_CR_and_Thru -v github.com/influxdata/influxdb/v2/tsdb/engine/tsm1 | grep "Compression time" >> $output_path
  cd ../..
done
awk '{ print $1 " " $5 " " $8 " " $11}' $dir/gorilla.log > $dir/gorilla.res

# test chimp
for i in "${!files[@]}"; do
  echo ${files[i]} | tr '\n' ' ' >> "$dir/chimp.log"
  yes | cp ${files[i]} code/influxdb/datasets/float.csv
  output_path=$(realpath $dir/chimp.log)
  cd code/influxdb
  git checkout chimp &> /dev/null
  go test -test.timeout 0 -run TestCompress_CR_and_Thru -v github.com/influxdata/influxdb/v2/tsdb/engine/tsm1 | grep "Compression time" >> $output_path
  cd ../..
done
awk '{ print $1 " " $5 " " $8 " " $11}' $dir/chimp.log > $dir/chimp.res

# prepare csv header
tmp_file=$(mktemp)
for i in "${!files[@]}"; do
  if [ $i -eq 0 ]; then
    printf "compress algo" > $tmp_file
  fi
  printf ",%s" $(basename ${files[i]}) >> $tmp_file
done

# compress ratio
cp $tmp_file $dir/compress_ratio.csv
for label in "${labels[@]}"; do
  awk -v label="$label" '{array[NR] = $2} \
  END {printf "\n%s", label; for (i=1; i<=NR; i++) { printf ",%s", array[i] }}' \
  $dir/${label}.res >> $dir/compress_ratio.csv
done

# compress throughput
cp $tmp_file $dir/compress_throughput.csv
for label in "${labels[@]}"; do
  awk -v label="$label" -v file_size="${file_size[*]}" 'BEGIN {split(file_size, size, " ")} \
    {array[NR] = $3} \
    END {printf "\n%s", label; for (i=1; i<=NR; i++) \
    { printf ",%.2f", size[i] * 1000 / 1024 / 1024 / strtonum(array[i]) }}'\
    $dir/${label}.res >> $dir/compress_throughput.csv
done

# decompress throughput
cp $tmp_file $dir/decompress_throughput.csv
for label in "${labels[@]}"; do
  awk -v label="$label" -v file_size="${file_size[*]}" 'BEGIN {split(file_size, size, " ")} \
    {array[NR] = $4} \
    END {printf "\n%s", label; for (i=1; i<=NR; i++) \
    { printf ",%.2f", size[i] * 1000 / 1024 / 1024 / strtonum(array[i]) }}'\
    $dir/${label}.res >> $dir/decompress_throughput.csv
done

# draw
python3 ./draw.py $dir
echo "result images are saved in $dir"

rm -rf $out_dir
