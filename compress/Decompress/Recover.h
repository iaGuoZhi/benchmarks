#include "AbstractDecompressor.h"
#include "GorillaXORDecompressor.h"
#include "ChimpXORDecompressor.h"
#include "ElfXORDecompressor.h"
#include "TimestampDecompressor.h"
#include "options.h"
#include "defs.h"

class Recover {
private:
  int lastBetaStar = __INT32_MAX__;
  CombOptions opts;
  ElfXORDecompressor elfXORDecompressor;
  AbstractDecompressor *decompressor;

  double nextValue();
  double recoverVByBetaStar();

  double Decompress() { return decompressor->readValue(); }
  int readInt(int len) {
    return read(decompressor->getReader(), len);
  }
  int getLength() { return decompressor->length; }
  bool useEraser() { return opts.getUseEraser(); }

public:
  Recover(uint8_t *in, size_t len, const char *options);
  int decompress(double *output);
};
