#include "AbstractXORCompressor.h"

class ElfXORCompressor : public AbstractXORCompressor {
private:
  int storedLeadingZeros = __INT32_MAX__;
  int storedTrailingZeros = __INT32_MAX__;
  uint64_t storedVal = 0;

  void writeFirst(long value);
  void compressValue(long value);
};
