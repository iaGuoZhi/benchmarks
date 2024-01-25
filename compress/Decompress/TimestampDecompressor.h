#include "AbstractDecompressor.h"

class TimestampDecompressor : public AbstractDecompressor {
private:
  DOUBLE prevVal = {.i = 0}, value = {.i = 0};
  uint64_t prevDelta = 0, delta = 0;

  double firstValue();
  uint64_t recoverInterval(uint64_t _d);
  double nextValue();
public:
  ~TimestampDecompressor() {}
};
