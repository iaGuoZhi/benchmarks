#include <gflags/gflags.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>


unsigned int default_buffer_size = 1024;
unsigned int default_number_of_ipc = 10000;

DEFINE_int32(buffer_size, default_buffer_size, "buffer size in Bytes");
DEFINE_int32(number_of_ipc, default_number_of_ipc, "number of operations");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  int ifds[2], ofds[2];
  unsigned long sec, usec;
  struct timeval tv1, tv2;
  struct timezone tz;
  char *buffer;
  int buffer_size = FLAGS_buffer_size;
  int number_of_ipc = FLAGS_number_of_ipc;

  pipe(ifds);
  pipe(ofds);
  buffer = (char *)malloc(buffer_size);
  pid_t pid = fork();
  if (pid == 0) {
    // child process
    for (int i = 0; i < number_of_ipc; i++) {
      if (read(ifds[0], buffer, buffer_size) != buffer_size) {
        std::cout << "child read error" << std::endl;
      }
      if (write(ofds[1], buffer, buffer_size) != buffer_size) {
        std::cout << "child write error" << std::endl;
      }
    }
  } else {
    // parent process
    gettimeofday(&tv1, &tz);
    for (int i = 0; i < number_of_ipc; i++) {
      if (write(ifds[1], buffer, buffer_size) != buffer_size) {
        std::cout << "parent write error" << std::endl;
      }
      if (read(ofds[0], buffer, buffer_size) != buffer_size) {
        std::cout << "parent read error" << std::endl;
      }
    }
    gettimeofday(&tv2, &tz);

    if (tv2.tv_usec < tv1.tv_usec) {
      usec = 1000000 + tv2.tv_usec - tv1.tv_usec;
      sec = tv2.tv_sec - tv1.tv_sec - 1;
    } else {
      usec = tv2.tv_usec - tv1.tv_usec;
      sec = tv2.tv_sec - tv1.tv_sec;
    }

    std::cout << "Buffer size: " << buffer_size << " bytes, time: " << sec << "."
              << usec << "s, latency: " << (sec * 1000000 + usec) * 1000.0 /
                 (number_of_ipc * 2) << "ns" << std::endl;
  }

  free(buffer);
  return 0;
}
