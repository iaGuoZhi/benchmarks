#include "AbstractDecompressor.h"

static const short leadingRepresentation[] = {0, 8, 12, 16, 18, 20, 22, 24};

class ElfXORDecompressor : public AbstractDecompressor {
private:
  DOUBLE storedVal = {.i = 0};
  int storedLeadingZeros = __INT32_MAX__;
  int storedTrailingZeros = __INT32_MAX__;

  double firstValue();
  double nextValue();
};
