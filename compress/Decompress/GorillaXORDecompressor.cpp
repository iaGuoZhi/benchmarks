#include "GorillaXORDecompressor.h"
#include <iostream>
#include <iomanip>

double GorillaXORDecompressor::firstValue() {
  prevVal.i = readLong(&reader, 64);
  if (opts.getIsTimestamp()) {
    return prevVal.i;
  }
  return prevVal.d;
}

uint64_t GorillaXORDecompressor::readXOR(BitReader *reader, uint64_t leading, uint64_t meaningful) {
  uint64_t trailing = 64 - leading - meaningful;
  uint64_t _xor = readLong(reader, meaningful);
  return _xor << trailing;
}

uint64_t GorillaXORDecompressor::recoverXOR(uint64_t _d){
  if (opts.getXORType() == CombOptions::XOR_type::Delta) {
    value.i = prevVal.i ^ _d;
  } else if (opts.getXORType() == CombOptions::XOR_type::DeltaOfDelta) {
    delta = prevDelta ^ _d;
    value.i = prevVal.i ^ delta;
    prevDelta = delta;
  }
  prevVal.i = value.i;
  return value.i;
}

uint64_t GorillaXORDecompressor::recoverInterval(uint64_t _d){
  if (opts.getXORType() == CombOptions::XOR_type::Delta) {
    value.i = prevVal.i + _d;
  } else if (opts.getXORType() == CombOptions::XOR_type::DeltaOfDelta) {
    delta = prevDelta + _d;
    value.i = prevVal.i + delta;
    prevDelta = delta;
  }
  prevVal.i = value.i;
  return value.i;
}

double GorillaXORDecompressor::nextFloatValue() {
  uint64_t _d;
  switch (peek(&reader, 2)) {
  case 0:
  case 1:
    forward(&reader, 1);
    value.i = recoverXOR(0);
    break;
  case 2:
    forward(&reader, 2);
    _d = readXOR(&reader, leading, meaningful);
    value.i = recoverXOR(_d);
    break;
  case 3:
    forward(&reader, 2);
    leading = read(&reader, 5);
    meaningful = read(&reader, 6) + 1;
    _d = readXOR(&reader, leading, meaningful);
    value.i = recoverXOR(_d);
    break;
  default:
    break;
  }
  return value.d;
}

double GorillaXORDecompressor::nextTimestampValue() {
  long _d;
  static int cnt = 1;
  if (read(&reader, 1) == 0) {
    _d = 0;
  } else if (read(&reader, 1) == 0) {
    _d = read(&reader, 7) - 63;
  } else if (read(&reader, 1) == 0) {
    _d = read(&reader, 9) - 255;
  } else if (read(&reader, 1) == 0) {
    _d = read(&reader, 12) - 2047;
  } else {
    _d = (int)read(&reader, 32);
  }

  value.i = recoverInterval(_d);
  return value.i;
}

double GorillaXORDecompressor::nextValue() {
  if (opts.getIsTimestamp()) {
    return nextTimestampValue();
  } else {
    return nextFloatValue();
  }
}
