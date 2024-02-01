sudo echo 1024 >  /proc/sys/vm/nr_hugepages
sudo mount hugetlbfs /mnt/huge -t hugetlbfs
