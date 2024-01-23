#include "GorillaXORDecompressor.h"

double GorillaXORDecompressor::firstValue() {
  prevVal.i = readLong(&reader, 64);
  return prevVal.d;
}

uint64_t GorillaXORDecompressor::read_delta(BitReader *reader, uint64_t leading, uint64_t meaningful) {
  uint64_t trailing = 64 - leading - meaningful;
  uint64_t delta = readLong(reader, meaningful);
  return delta << trailing;
}

double GorillaXORDecompressor::nextValue() {
  switch (peek(&reader, 2)) {
  case 0:
  case 1:
    forward(&reader, 1);
    value.d = prevVal.d;
    break;
  case 2:
    forward(&reader, 2);
    delta = read_delta(&reader, leading, meaningful);
    value.i = (prevVal.i) ^ delta;
    break;
  case 3:
    forward(&reader, 2);
    leading = read(&reader, 5);
    meaningful = read(&reader, 6) + 1;
    delta = read_delta(&reader, leading, meaningful);
    value.i = (prevVal.i) ^ delta;
    break;
  default:
    break;
  }
  prevVal.i = value.i;
  return value.d;
}
