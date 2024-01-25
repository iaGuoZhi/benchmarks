#include "AbstractDecompressor.h"

class GorillaXORDecompressor : public AbstractDecompressor {
private:
  DOUBLE prevVal = {.i = 0}, value = {.i = 0};
  uint64_t prevDelta = 0, delta = 0;
  uint64_t leading, meaningful;

  double firstValue();
  uint64_t readXOR(BitReader *reader, uint64_t leading, uint64_t meaningful);
  uint64_t recoverXOR(uint64_t _d);
  double nextValue();
};
