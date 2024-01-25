#include "GorillaXORCompressor.h"
#include <iostream>
#include <iomanip>

void GorillaXORCompressor::writeFirst(long value) {
  if (opts.getIsTimestamp()) {
    DOUBLE _value = {.i = (uint64_t) value};
    value = (long) _value.d;
  }
  writeLong(&writer, value, 64);
  length = 1;
  prevVal = value;
}

uint64_t GorillaXORCompressor::calculateXOR(long value) {
  uint64_t _d;
  if (opts.getXORType() == CombOptions::XOR_type::Delta) {
    delta = value ^ prevVal;
    _d = delta;
  } else if (opts.getXORType() == CombOptions::XOR_type::DeltaOfDelta) {
    delta = value ^ prevVal;
    _d = delta ^ prevDelta;
    prevDelta = delta;
  }
  prevVal = value;
  return _d;
}

uint64_t GorillaXORCompressor::calculateInterval(long value) {
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

void GorillaXORCompressor::compressInterval(long _d) {
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
  } else
  {
    write(&writer, 15, 4);
    write(&writer, _d, 32);
  }
}

void GorillaXORCompressor::compressXOR(uint64_t _d) {
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
}

void GorillaXORCompressor::compressValue(long value) {
  if (opts.getIsTimestamp()) {
    long _d = calculateInterval(value);
    compressInterval(_d);
  } else {
    uint64_t _d = calculateXOR(value);
    compressXOR(_d);
  }
  length++;
}
