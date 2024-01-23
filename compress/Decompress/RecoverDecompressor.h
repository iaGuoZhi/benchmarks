#include "AbstractXORDecompressor.h"
#include "GorillaXORDecompressor.h"
#include "ChimpXORDecompressor.h"
#include "ElfXORDecompressor.h"
#include "options.h"
#include "defs.h"

class RecoverDecompressor {
private:
  int lastBetaStar = __INT32_MAX__;
  CombOptions opts;
  ElfXORDecompressor elfXORDecompressor;
  AbstractXORDecompressor *xorDecompressor;

  double nextValue();
  double recoverVByBetaStar();

  double xorDecompress() { return xorDecompressor->readValue(); }
  int readInt(int len) {
    return read(xorDecompressor->getReader(), len);
  }
  int getLength() { return xorDecompressor->length; }
  bool useEraser() { return opts.getUseEraser(); }

public:
  RecoverDecompressor(uint8_t *in, size_t len, const char *options);
  int decompress(double *output);
};
