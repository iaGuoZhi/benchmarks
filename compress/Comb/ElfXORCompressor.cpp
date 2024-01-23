#include "ElfXORCompressor.h"

static const short leadingRepresentation[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5,
    6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static const short leadingRound[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  8,  8,  8,  8,  12, 12, 12, 12,
    16, 16, 18, 18, 20, 20, 22, 22, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24};

void ElfXORCompressor::writeFirst(long value) {
  storedVal = value;
  length = 1;
  int trailingZeros = __builtin_ctzl(value);
  write(&writer, trailingZeros, 7);
  if (value != 0) {
    writeLong(&writer, storedVal >> (trailingZeros + 1), 63 - trailingZeros);
  }
}

void ElfXORCompressor::compressValue(long value) {
  uint64_t _xor = storedVal ^ value;
  if (_xor == 0) {
    write(&writer, 1, 2);
  } else {
    int leadingZeros = leadingRound[__builtin_clzl(_xor)];
    int trailingZeros = __builtin_ctzl(_xor);

    if (leadingZeros == storedLeadingZeros &&
        trailingZeros >= storedTrailingZeros) {
      int centerBits = 64 - storedLeadingZeros - storedTrailingZeros;
      int len = 2 + centerBits;
      if (len > 64) {
        write(&writer, 0, 2);
        writeLong(&writer, _xor >> storedTrailingZeros, centerBits);
      } else {
        writeLong(&writer, _xor >> storedTrailingZeros, len);
      }
    } else {
      storedLeadingZeros = leadingZeros;
      storedTrailingZeros = trailingZeros;
      int centerBits = 64 - storedLeadingZeros - storedTrailingZeros;

      if (centerBits <= 16) {
        write(
            &writer,
            (((0x2 << 3) | leadingRepresentation[storedLeadingZeros]) << 4) |
                (centerBits & 0xf),
            9);
        writeLong(&writer, _xor >> (storedTrailingZeros + 1), centerBits - 1);
      } else {
        write(
            &writer,
            (((0x3 << 3) | leadingRepresentation[storedLeadingZeros]) << 6) |
                (centerBits & 0x3f),
            11);
        writeLong(&writer, _xor >> (storedTrailingZeros + 1), centerBits - 1);
      }
    }
    storedVal = value;
  }
  length++;

}
