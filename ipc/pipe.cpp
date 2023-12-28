#include <gflags/gflags.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <cstring>


unsigned int default_buffer_size = 1024;
unsigned int default_number_of_ipc = 10000;

DEFINE_int32(buffer_size, default_buffer_size, "buffer size in Bytes");
DEFINE_int32(number_of_ipc, default_number_of_ipc, "number of operations");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  int fds[2];
  unsigned long sec, usec;
  struct timeval tv1, tv2;
  struct timezone tz;
  char *buffer;
  int buffer_size = FLAGS_buffer_size;
  int number_of_ipc = FLAGS_number_of_ipc;

  pipe(fds);
  buffer = (char *)malloc(buffer_size);
  pid_t pid = fork();
  if (pid == 0) {
    // child process
    for (int i = 0; i < number_of_ipc; i++) {
      char ch = 'a' + i % 26;
      memset(buffer, ch, buffer_size);
      if (write(fds[1], buffer, buffer_size) != buffer_size) {
        std::cout << "child write error" << std::endl;
      }
    }
  } else {
    // parent process
    char dest[buffer_size];
    gettimeofday(&tv1, &tz);
    for (int i = 0; i < number_of_ipc; i++) {
      if (read(fds[0], buffer, buffer_size) != buffer_size) {
        std::cout << "parent read error" << std::endl;
      }
      memcpy(dest, buffer, buffer_size);
      //std::cout << *dest << std::endl;
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
                 number_of_ipc << "ns" << std::endl;
  }

  free(buffer);
  return 0;
}
