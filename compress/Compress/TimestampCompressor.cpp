#include "TimestampCompressor.h"
#include <iostream>
#include <iomanip>

void TimestampCompressor::writeFirst(long value) {
  DOUBLE _value = {.i = (uint64_t) value};
  value = (long) _value.d;
  writeLong(&writer, value, 64);
  length = 1;
  prevVal = value;
}

uint64_t TimestampCompressor::calculateInterval(long value) {
  uint64_t _d;
  // get value from double, if it is a timestamp
  DOUBLE _value = {.i = (uint64_t) value};
  value = _value.d;
  if (opts.getXORType() == CombOptions::XOR_type::Delta) {
    _d = value - prevVal;
  } else if (opts.getXORType() == CombOptions::XOR_type::DeltaOfDelta) {
    delta = value - prevVal;
    _d = delta - prevDelta;
    prevDelta = delta;
  }
  prevVal = value;
  return _d;
}

void TimestampCompressor::compressValue(long value) {
  long _d = calculateInterval(value);
  if (_d == 0)
  {
    write(&writer, 0, 1);
  } else if ((-63 <= _d) && (_d <= 64))
  {
    write(&writer, 2, 2);
    write(&writer, _d + 63, 7);
  } else if ((-255 <= _d) && (_d <= 256))
  {
    write(&writer, 6, 3);
    write(&writer, _d + 255, 9);
  } else if ((-2047 <= _d) && (_d <= 2048))
  {
    write(&writer, 14, 4);
    write(&writer, _d + 2047, 12);
  } else if ((-65535 <= _d) && (_d <= 65536))
  {
    write(&writer, 30, 5);
    write(&writer, _d + 65535, 17);
  } else {
    write(&writer, 31, 5);
    write(&writer, _d, 32);
  }
  length++;
}
