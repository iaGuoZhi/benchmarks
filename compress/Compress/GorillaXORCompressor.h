#include "AbstractXORCompressor.h"

class GorillaXORCompressor : public AbstractXORCompressor {
private:
  uint64_t prevLeading = -1L;
  uint64_t prevTrailing = 0;
  uint64_t prevVal = 0;
  uint64_t prevDelta = 0, delta = 0;
  uint64_t deltaOfDelta = 0;
  uint64_t leading, trailing;
  uint64_t mask;
  double sum;

  uint64_t calculateD(long value);
  uint64_t calculateXOR(long value);
  uint64_t calculateInterval(long value);
  void writeFirst(long value);
  void compressValue(long value);
};
