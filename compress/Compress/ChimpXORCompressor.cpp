#include "ChimpXORCompressor.h"

static const uint16_t leadingRep[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5,
      6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static const uint16_t leadingRnd[] = {
      0,  0,  0,  0,  0,  0,  0,  0,  8,  8,  8,  8,  12, 12, 12, 12,
      16, 16, 18, 18, 20, 20, 22, 22, 24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24};

void ChimpXORCompressor::writeFirst(long value) {
  writeLong(&writer, value, 64);
  storedValues[current] = value;
  indices[((int)value) & setLsb] = index;
  length = 1;
}

void ChimpXORCompressor::compressValue(long value) {
  int32_t key = (int)value & setLsb;
  int64_t delta;
  int32_t previousIndex;
  int32_t trailingZeros = 0;
  int32_t currIndex = indices[key];
  if ((index - currIndex) < previousValues) {
    delta = value ^ storedValues[currIndex % previousValues];
    trailingZeros = __builtin_ctzl(delta);
    if (trailingZeros > threshold) {
      previousIndex = currIndex % previousValues;
    } else {
      previousIndex = index % previousValues;
      delta = storedValues[previousIndex] ^ value;
    }
  } else {
    previousIndex = index % previousValues;
    delta = storedValues[previousIndex] ^ value;
  }

  if (delta == 0) {
    // {0_0_previousIndex}
    write(&writer, previousIndex, flagZeroSize);
    storedLeadingZeros = 65;
  } else {
    int32_t leadingZeros = leadingRnd[__builtin_clzl(delta)];

    if (trailingZeros > threshold) {
      int32_t significantBits = 64 - leadingZeros - trailingZeros;
      // {0_1_previousIndex_leadingZeros_significantBits}
      write(&writer,
            ((previousValues + previousIndex) << 9) |
                (leadingRep[leadingZeros] << 6) | significantBits,
            flagOneSize);

      writeLong(&writer, delta >> trailingZeros, significantBits);
      storedLeadingZeros = 65;
    } else if (leadingZeros == storedLeadingZeros) {
      // {10_delta}
      write(&writer, 2, 2);
      int32_t significantBits = 64 - leadingZeros;
      writeLong(&writer, delta, significantBits);
    } else {
      // {11_leadingZeros_delta}
      storedLeadingZeros = leadingZeros;
      int significantBits = 64 - leadingZeros;
      write(&writer, (0x3 << 3) | leadingRep[leadingZeros], 5);
      writeLong(&writer, delta, significantBits);
    }
  }
  current = (current + 1) % previousValues;
  storedValues[current] = value;
  index++;
  indices[key] = index;
  length++;
}

ChimpXORCompressor::ChimpXORCompressor() {
  indices = (int32_t *)calloc(sizeof(int32_t), (1 << (threshold + 1)));
  storedValues = (int64_t *)calloc(sizeof(int64_t), PREVIOUS_VALUES);
}

ChimpXORCompressor::~ChimpXORCompressor() {
  free(indices);
  free(storedValues);
}
