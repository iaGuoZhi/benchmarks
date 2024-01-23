#include "AbstractXORCompressor.h"

class GorillaXORCompressor : public AbstractXORCompressor {
private:
  uint64_t prevLeading = -1L;
  uint64_t prevTrailing = 0;
  uint64_t prevVal = 0;
  uint64_t leading, trailing;
  uint64_t mask;
  double sum;

  void writeFirst(long value);
  void compressValue(long value);
};
