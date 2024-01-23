#include "AbstractXORDecompressor.h"

class GorillaXORDecompressor : public AbstractXORDecompressor {
private:
  DOUBLE prevVal = {.i = 0}, value = {.i = 0};
  uint64_t leading, meaningful, delta;

  double firstValue();
  uint64_t read_delta(BitReader *reader, uint64_t leading, uint64_t meaningful);
  double nextValue();
};
