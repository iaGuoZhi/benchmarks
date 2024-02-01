#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int mmap_file() {
    size_t hugepage_size = 4 * 1024 * 1024;

    int fd = open("/mnt/huge/test.txt", O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        std::cout << "Failed to open the file" << std::endl;
        return 1;
    }

    void* ptr = mmap(nullptr, hugepage_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HUGETLB, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cout << "Failed to allocate huge page" << std::endl;
        close(fd);
        return 1;
    }

    // HugePages_Free--
    ((char*)ptr)[0] = 'a';
    // HugePages_Free--
    ((char*)ptr)[2 * 1024 * 1024] = 'b';

    munmap(ptr, hugepage_size);
    close(fd);

    return 0;
}

int mmap_shm() {
  size_t hugepage_size = 4 * 1024 * 1024;

  void *ptr = mmap(nullptr, hugepage_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    std::cout << "Failed to allocate huge page" << std::endl;
    return 1;
  }
  madvise(ptr, hugepage_size, MADV_HUGEPAGE);

  // HugePages_Free--
  ((char*)ptr)[0] = 'a';
  // HugePages_Free--
  ((char*)ptr)[2 * 1024 * 1024] = 'b';
  sleep(10);
  return 0;
}


int main() {
  mmap_file();
  mmap_shm();
}
