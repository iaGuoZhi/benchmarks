#include "AbstractXORDecompressor.h"

class ChimpXORDecompressor : public AbstractXORDecompressor {
private:
  DOUBLE prevVal = {.i = 0}, value = {.i = 0};
  const int PREVIOUS_VALUES = 128;
  int32_t previousValues = PREVIOUS_VALUES;
  int32_t previousValuesLog2 = 31 - __builtin_clz(PREVIOUS_VALUES);
  int32_t storedLeadingZeros = INT32_MAX;
  int32_t storedTrailingZeros = 0;
  int32_t initialFill = previousValuesLog2 + 9;
  int64_t *storedValues;
  uint32_t tmp, fill, index, significantBits;
  int32_t i = 0;

  int64_t delta;

  double firstValue();
  double nextValue();

public:
  ChimpXORDecompressor();
  ~ChimpXORDecompressor();
};
