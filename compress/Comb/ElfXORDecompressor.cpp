#include "ElfXORDecompressor.h"

double ElfXORDecompressor::firstValue() {
  int trailingZeros = read(&reader, 7);
  if (trailingZeros < 64) {
    storedVal.i = ((readLong(&reader, 63 - trailingZeros) << 1) + 1)
                  << trailingZeros;
  } else {
    storedVal.i = 0;
  }
  return storedVal.d;
}

double ElfXORDecompressor::nextValue() {
  long value;
  int centerBits;
  uint32_t leadAndCenter;
  int flag = read(&reader, 2);
  switch (flag) {
  case 3:
    leadAndCenter = read(&reader, 9);
    storedLeadingZeros = leadingRepresentation[leadAndCenter >> 6];
    centerBits = leadAndCenter & 0x3f;
    if (centerBits == 0) {
      centerBits = 64;
    }
    storedTrailingZeros = 64 - storedLeadingZeros - centerBits;
    value = ((readLong(&reader, centerBits - 1) << 1) + 1)
            << storedTrailingZeros;
    value = storedVal.i ^ value;
    storedVal.i = value;
    break;
  case 2:
    leadAndCenter = read(&reader, 7);
    storedLeadingZeros = leadingRepresentation[leadAndCenter >> 4];
    centerBits = leadAndCenter & 0xf;
    if (centerBits == 0) {
      centerBits = 16;
    }
    storedTrailingZeros = 64 - storedLeadingZeros - centerBits;
    value = ((readLong(&reader, centerBits - 1) << 1) + 1)
            << storedTrailingZeros;
    value = storedVal.i ^ value;
    storedVal.i = value;
    break;
  case 1:
    break;
  default:
    centerBits = 64 - storedLeadingZeros - storedTrailingZeros;
    value = readLong(&reader, centerBits) << storedTrailingZeros;
    value = storedVal.i ^ value;
    storedVal.i = value;
    break;
  }
  return storedVal.d;
}
