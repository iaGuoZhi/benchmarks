#include "GorillaXORCompressor.h"

void GorillaXORCompressor::writeFirst(long value) {
  writeLong(&writer, value, 64);
  length = 1;
  prevVal = value;
}

void GorillaXORCompressor::compressValue(long value) {
  uint64_t vDelta = value ^ prevVal;
  prevVal = value;
  if (vDelta == 0) {
    write(&writer, 0, 1);
  } else {
    leading = __builtin_clzl(vDelta);
    trailing = __builtin_ctzl(vDelta);

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
      write(&writer, vDelta >> prevTrailing, l);
    } else {
      write(&writer, vDelta >> 32, 32 - prevLeading);
      write(&writer, vDelta >> prevTrailing, 32 - prevTrailing);
    }
  }
  length++;
}
