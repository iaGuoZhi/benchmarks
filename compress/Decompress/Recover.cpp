#include "Recover.h"

double Recover::nextValue() {
  double v;
  if (!useEraser()) {
    v = Decompress();
  } else {
    if (readInt(1) == 0) {
      v = recoverVByBetaStar();
    } else if (readInt(1) == 0) {
      v = Decompress();
    } else {
      lastBetaStar = readInt(4);
      v = recoverVByBetaStar();
    }
  }
  return v;
}

double Recover::recoverVByBetaStar() {
  double v;
  double vPrime = Decompress();
  int sp = getSP(abs(vPrime));
  if (lastBetaStar == 0) {
    v = get10iN(-sp - 1);
    if (vPrime < 0) {
      v = -v;
    }
  } else {
    int alpha = lastBetaStar - sp - 1;
    v = roundUp(vPrime, alpha);
  }
  return v;
}

Recover::Recover(uint8_t *in, size_t len, const char *options) {
  if (options != NULL) {
    opts = CombOptions(options);
  }
  switch (opts.getCompressAlgo()) {
    case CombOptions::Compress_algo::Gorilla:
      decompressor = new GorillaXORDecompressor();
      break;
    case CombOptions::Compress_algo::Chimp:
      decompressor = new ChimpXORDecompressor();
      break;
    case CombOptions::Compress_algo::Elf:
      decompressor = new ElfXORDecompressor();
      break;
    case CombOptions::Compress_algo::Timestamp:
      decompressor = new TimestampDecompressor();
      break;
    default:
      decompressor = new GorillaXORDecompressor();
      break;
  }
  decompressor->init((uint32_t *)in, len / 4, opts);
}

int Recover::decompress(double *output) {
  int len = getLength();
  for (int i = 0; i < len; i++) {
    if (i == 4219) {
      asm("nop");
    }
    output[i] = nextValue();
  }
  return len;
}
