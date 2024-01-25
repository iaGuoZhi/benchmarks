#include "AbstractCompressor.h"

class TimestampCompressor : public AbstractCompressor {
private:
  uint64_t prevVal = 0;
  uint64_t prevDelta = 0, delta = 0;

  uint64_t calculateInterval(long value);
  void writeFirst(long value);
  void compressInterval(long _d);
  void compressValue(long value);
};
