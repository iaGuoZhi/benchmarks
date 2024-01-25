#include <stdlib.h>
#include "options.h"
#include "defs.h"
#include "AbstractCompressor.h"

class Eraser {
private:
  int lastBetaStar = __INT32_MAX__;
  CombOptions opts;
  AbstractCompressor *compressor;

  int writeInt(int n, int len);
  int writeBit(bool bit);
  void xorCompress(long vPrimeLong);
  bool useEraser();

public:
  Eraser(const char *options);
  void init(int length);
  uint32_t *getBytes();
  void close();
  int getSize();
  void addValue(double v);
};
