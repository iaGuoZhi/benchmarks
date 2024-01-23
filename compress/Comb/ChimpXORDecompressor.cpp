#include "ChimpXORDecompressor.h"

static const int16_t leadingRep[] = {0, 8, 12, 16, 18, 20, 22, 24};

double ChimpXORDecompressor::firstValue() {
  prevVal.i = readLong(&reader, 64);
  storedValues[i++] = prevVal.i;
  return prevVal.d;
}

double ChimpXORDecompressor::nextValue() {
  switch (read(&reader, 2)) {
  case 3:
    tmp = read(&reader, 3);
    storedLeadingZeros = leadingRep[tmp];
    delta = readLong(&reader, 64 - storedLeadingZeros);
    value.i = prevVal.i ^ delta;
    storedValues[i % previousValues] = value.i;
    break;
  case 2:
    delta = readLong(&reader, 64 - storedLeadingZeros);
    value.i = prevVal.i ^ delta;
    storedValues[i % previousValues] = value.i;
    break;
  case 1:
    fill = initialFill;
    tmp = read(&reader, fill);
    fill -= previousValuesLog2;
    index = (tmp >> fill) & ((1 << previousValuesLog2) - 1);
    fill -= 3;
    storedLeadingZeros = leadingRep[(tmp >> fill) & 0x7];
    fill -= 6;
    significantBits = (tmp >> fill) & 0x3f;
    if (significantBits == 0) {
      significantBits = 64;
    }
    storedTrailingZeros = 64 - significantBits - storedLeadingZeros;
    delta = readLong(&reader, significantBits);
    value.i = storedValues[index] ^ (delta << storedTrailingZeros);
    storedValues[i % previousValues] = value.i;
    break;
  default:
    value.i = storedValues[readLong(&reader, previousValuesLog2)];
    storedValues[i % previousValues] = value.i;
    break;
  }
  prevVal.i = value.i;
  i++;
  return value.d;
}

ChimpXORDecompressor::ChimpXORDecompressor() {
  storedValues = (int64_t *)calloc(sizeof(int64_t), previousValues);
}

ChimpXORDecompressor::~ChimpXORDecompressor() {
  free(storedValues);
}
