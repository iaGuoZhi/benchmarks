#include <unistd.h>
#include <cstdlib>
#include <iostream>

int num = 0;

template <typename T, T... Is>
void execute_helper(std::integer_sequence<T, Is...>) {
  ((Is, num++), ...);
}

template <typename T, T N>
void repeat() {
  execute_helper(std::make_integer_sequence<T, N>{});
}

int main() {
  repeat<int, 1000000>();
}
