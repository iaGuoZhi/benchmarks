#include "AbstractXORCompressor.h"

class ChimpXORCompressor : public AbstractXORCompressor {
private:
  const int32_t PREVIOUS_VALUES = 128;
  int32_t storedLeadingZeros = INT32_MAX;
  int32_t index = 0, current = 0;
  int32_t previousValues = PREVIOUS_VALUES;
  int32_t previousValuesLog2 = 31 - __builtin_clz(PREVIOUS_VALUES);
  int32_t threshold = 6 + previousValuesLog2;
  int32_t setLsb = (1 << (threshold + 1)) - 1;
  int32_t *indices;
  int64_t *storedValues;
  int32_t flagZeroSize = 1 + 1 + previousValuesLog2;
  int32_t flagOneSize = 1 + 1 + previousValuesLog2 + 3 + 6;

  void writeFirst(long value);
  void compressValue(long value);

public:
  ChimpXORCompressor();
  ~ChimpXORCompressor();
};
