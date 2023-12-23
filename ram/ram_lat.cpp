#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#define DELTAS_SZ 32768 * 4

#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
inline static uint64_t realtime_now() {
  struct timespec now_ts;
  clock_gettime(CLOCK_MONOTONIC, &now_ts);
  return TIMESPEC_NSEC(&now_ts);
}

int main(int argc, char* argv[]) {
  (void)argc;
  uint64_t base, rt0, rt1, i;
  struct {
    uint32_t t;  // timestamp
    uint32_t d;  // duration
  } deltas[DELTAS_SZ];

  void* addr =
      mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED) {
    perror("mmap()");
    exit(-1);
  }

  int* one_global_var = (int*)addr;
  _mm_clflush(one_global_var);

  base = realtime_now();
  rt0 = base;

  for (i = 0; i < DELTAS_SZ; i++) {
    // Perform memory load.
    *(volatile int*)one_global_var;

    // Flush CPU cache. This is relatively slow
    _mm_clflush(one_global_var);

    // mfence is needed, otherwise sometimes the loop
    // takes very short time (25ns instead of like 160). I
    // blame reordering.
    asm volatile("mfence");

    rt1 = realtime_now();
    uint64_t td = rt1 - rt0;
    rt0 = rt1;
    deltas[i].t = rt1 - base;
    deltas[i].d = td;
  }
  for (i = 0; i < DELTAS_SZ; i++) {
    printf("%u,%u\n", deltas[i].t, deltas[i].d);
  }

  return 0;
}
