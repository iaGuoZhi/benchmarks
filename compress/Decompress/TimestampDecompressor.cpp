#include "TimestampDecompressor.h"
#include <iostream>
#include <iomanip>

double TimestampDecompressor::firstValue() {
  prevVal.i = readLong(&reader, 64);
  return prevVal.i;
}

uint64_t TimestampDecompressor::recoverInterval(uint64_t _d){
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

double TimestampDecompressor::nextValue() {
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
  } else if (read(&reader, 1) == 0) {
    _d = read(&reader, 17) - 65535;
  } else {
    _d = (int)read(&reader, 32);
  }

  value.i = recoverInterval(_d);
  return value.i;
}
