#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <iomanip>

double calculate() {
  double e = 1;
  double cur = 1;

  for (int i = 1; i < 1000000; i++) {
   cur *= i;
   e += 1 / cur;
  }

  return e;
}

int main() {
  struct timeval start, end;
  double e;

  gettimeofday(&start, NULL);
  e = calculate();
  gettimeofday(&end, NULL);

  std::cout << std::setprecision (100) << "e = " << e << std::endl;
  std::cout << "Time elapsed: " << (end.tv_sec - start.tv_sec) * 1000000 +
                                       (end.tv_usec - start.tv_usec)
                                << " us" << std::endl;

}
