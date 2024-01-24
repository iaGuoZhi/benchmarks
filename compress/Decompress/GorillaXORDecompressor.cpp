#include "GorillaXORDecompressor.h"
#include <iostream>
#include <iomanip>

double GorillaXORDecompressor::firstValue() {
  prevVal.i = readLong(&reader, 64);
  return prevVal.d;
}

uint64_t GorillaXORDecompressor::readXOR(BitReader *reader, uint64_t leading, uint64_t meaningful) {
  uint64_t trailing = 64 - leading - meaningful;
  uint64_t _xor = readLong(reader, meaningful);
  return _xor << trailing;
}

uint64_t GorillaXORDecompressor::recoverD(uint64_t _d){
  if (opts.getIsTimestamp()) {
    return recoverInterval(_d);
  } else {
    return recoverXOR(_d);
  }
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

double GorillaXORDecompressor::nextValue() {
  uint64_t _d;
  switch (peek(&reader, 2)) {
  case 0:
  case 1:
    forward(&reader, 1);
    value.i = recoverD(0);
    break;
  case 2:
    forward(&reader, 2);
    _d = readXOR(&reader, leading, meaningful);
    value.i = recoverD(_d);
    break;
  case 3:
    forward(&reader, 2);
    leading = read(&reader, 5);
    meaningful = read(&reader, 6) + 1;
    _d = readXOR(&reader, leading, meaningful);
    value.i = recoverD(_d);
    break;
  default:
    break;
  }
  return value.d;
}
