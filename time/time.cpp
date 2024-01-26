#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>

#include <iostream>

void do_some_stuff() {
  int sum = 0;
  for (int i = 0; i < 1000000000; ++i) {
    sum += i;
  }
}

#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
inline static uint64_t realtime_now() {
  struct timespec now_ts;
  clock_gettime(CLOCK_MONOTONIC, &now_ts);
  return TIMESPEC_NSEC(&now_ts);
}

inline uint64_t rdtsc() {
	uint32_t lo, hi;
	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)lo | ((uint64_t)hi << 32));
}

inline uint64_t cpu_freq() {
  uint64_t start = rdtsc();
  sleep(1);
  uint64_t end = rdtsc();
  return end - start;
}

inline uint64_t read_pmc(int counter) {
  uint32_t low, high;
  __asm__ volatile("rdpmc" : "=a" (low), "=d" (high) : "c" (counter));
  return ((uint64_t)high << 32) | low;
}

int main() {
  struct timeval start, end;
  uint64_t start_ns, end_ns;
  uint64_t start_cycles, end_cycles;
  uint64_t freq;

  // gettimeofday
  printf("========== gettimeofday ==========\n");
  gettimeofday(&start, NULL);
  do_some_stuff();
  gettimeofday(&end, NULL);
  std::cout << "Time elapsed: " << (end.tv_sec - start.tv_sec) * 1000000 +
                                       (end.tv_usec - start.tv_usec)
                                << " us" << std::endl;

  // clock_gettime
  printf("========== clock_gettime ==========\n");
  start_ns = realtime_now();
  do_some_stuff();
  end_ns = realtime_now();
  std::cout << "Time elapsed: " << (end_ns - start_ns) / 1000 << " us"
            << std::endl;

  // rdtsc
  printf("========== rdtsc ==========\n");
  start_cycles = rdtsc();
  do_some_stuff();
  end_cycles = rdtsc();
  freq = cpu_freq();
  std::cout << "Time elapsed: " << (end_cycles - start_cycles) * 1000000 / freq
            << " us" << std::endl;

  // clock
  printf("========== clock ==========\n");
  start_cycles = clock();
  do_some_stuff();
  end_cycles = clock();
  std::cout << "Time elapsed: " << (end_cycles - start_cycles) * 1000000 /
                                       CLOCKS_PER_SEC
            << " us" << std::endl;

  // pmu
  // printf("========== pmu ==========\n");
  // start_cycles = read_pmc(0);
  // do_some_stuff();
  // end_cycles = read_pmc(0);
  // std::cout << "Time elapsed: " << (end_cycles - start_cycles) * 1000000 /
  //                                      cpu_freq()
  //           << " us" << std::endl;
  //
  return 0;
}
