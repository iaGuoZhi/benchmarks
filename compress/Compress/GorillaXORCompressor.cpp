#include "GorillaXORCompressor.h"
#include <iostream>
#include <iomanip>

void GorillaXORCompressor::writeFirst(long value) {
  writeLong(&writer, value, 64);
  length = 1;
  prevVal = value;
}

uint64_t GorillaXORCompressor::calculateXOR(long value) {
  uint64_t _d;
  if (opts.getXORType() == CombOptions::XOR_type::DeltaOfDelta) {
    delta = value ^ prevVal;
    _d = delta ^ prevDelta;
    prevDelta = delta;
  } else {
    delta = value ^ prevVal;
    _d = delta;
  }

  prevVal = value;
  return _d;
}

void GorillaXORCompressor::compressValue(long value) {
  uint64_t _d = calculateXOR(value);
  if (_d == 0) {
    write(&writer, 0, 1);
  } else {
    leading = __builtin_clzl(_d);
    trailing = __builtin_ctzl(_d);

    leading = (leading >= 32) ? 31 : leading;
    uint64_t l;

    if (prevLeading != -1L && leading >= prevLeading &&
        trailing >= prevTrailing) {
      write(&writer, 2, 2);
      l = 64 - prevLeading - prevTrailing;
    } else {
      prevLeading = leading;
      prevTrailing = trailing;
      l = 64 - leading - trailing;

      write(&writer, 3, 2);
      write(&writer, leading, 5);
      write(&writer, l - 1, 6);
    }

    if (l <= 32) {
      write(&writer, _d >> prevTrailing, l);
    } else {
      write(&writer, _d >> 32, 32 - prevLeading);
      write(&writer, _d >> prevTrailing, 32 - prevTrailing);
    }
  }
  length++;
}
