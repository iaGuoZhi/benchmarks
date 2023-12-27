#include <gflags/gflags.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <utility>

char** p;

template <typename T, T... Is>
void execute_helper(std::integer_sequence<T, Is...>) {
  ((Is, p = (char**)*p), ...);
}

template <typename T, T N>
void repeat() {
  execute_helper(std::make_integer_sequence<T, N>{});
}

/* 1/4 LLC size of skylake, 1/5 of broadwell */
unsigned long default_buffer_size = 0x8192;
/* memsize / 64 * 8 */
unsigned long default_number_of_reads = 1048576;
/* skipped amount of memory before the next access */
unsigned long default_stride = 64;

DEFINE_int32(buffer_size, default_buffer_size, "buffer size in KB");
DEFINE_int32(number_of_reads, default_number_of_reads, "number of reads");
DEFINE_int32(stride, default_stride, "stride skipped before the next access");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  unsigned long i, j, size, tmp;
  unsigned long memsize = FLAGS_buffer_size * 1024;
  unsigned long count = FLAGS_number_of_reads;
  unsigned int stride = FLAGS_stride;
  unsigned long sec, usec;
  struct timeval tv1, tv2;
  // uint64_t t_start, t_end;
  struct timezone tz;
  unsigned int* indices;

  char* mem = (char*)mmap(NULL, memsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  size = memsize / stride;
  indices = (unsigned int*)malloc(size * sizeof(unsigned int));

  for (i = 0; i < size; i++)
    indices[i] = i;
  // shuffle indices
  for (i = 0; i < size; i++) {
    j = i + rand() % (size - i);
    if (i != j) {
      tmp = indices[i];
      indices[i] = indices[j];
      indices[j] = tmp;
    }
  }

  // fill mem with pointer references
  for (i = 0; i < size - 1; i++)
    *(char**)&mem[indices[i] * stride] = (char*)&mem[indices[i + 1] * stride];
  *(char**)&mem[indices[size - 1] * stride] = (char*)&mem[indices[0] * stride];

  p = (char**)mem;
  tmp = count / 1;

  gettimeofday(&tv1, &tz);
  for (i = 0; i < tmp; ++i) {
    repeat<int, 100>();
  }
  gettimeofday(&tv2, &tz);

  /* touch pointer p to prevent compiler optimization */
  char** touch = p;
  **touch = '\0';

  // std::cout << t_end - t_start << " ns" << std::endl;
  if (tv2.tv_usec < tv1.tv_usec) {
    usec = 1000000 + tv2.tv_usec - tv1.tv_usec;
    sec = tv2.tv_sec - tv1.tv_sec - 1;
  } else {
    usec = tv2.tv_usec - tv1.tv_usec;
    sec = tv2.tv_sec - tv1.tv_sec;
  }

  std::cout << "Buffer size: " << memsize / 1024 << " KB, stride " << stride << ", time " << sec
            << "." << usec << " s, latency " << (sec * 1000000 + usec) * 1000.0 / (tmp * 100)
            << " ns\n";
  munmap(mem, memsize);
  free(indices);
  return 0;
}
