#include "RecoverDecompressor.h"

double RecoverDecompressor::nextValue() {
  double v;
  if (!useEraser()) {
    v = xorDecompress();
  } else {
    if (readInt(1) == 0) {
      v = recoverVByBetaStar();
    } else if (readInt(1) == 0) {
      v = xorDecompress();
    } else {
      lastBetaStar = readInt(4);
      v = recoverVByBetaStar();
    }
  }
  return v;
}

double RecoverDecompressor::recoverVByBetaStar() {
  double v;
  double vPrime = xorDecompress();
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

RecoverDecompressor::RecoverDecompressor(uint8_t *in, size_t len, const char *options) {
  if (options != NULL) {
    opts = CombOptions(options);
  }
  switch (opts.getCompressAlgo()) {
    case CombOptions::Compress_algo::Gorilla:
      xorDecompressor = new GorillaXORDecompressor();
      break;
    case CombOptions::Compress_algo::Chimp:
      xorDecompressor = new ChimpXORDecompressor();
      break;
    case CombOptions::Compress_algo::Elf:
      xorDecompressor = new ElfXORDecompressor();
      break;
    default:
      xorDecompressor = new ElfXORDecompressor();
      break;
  }
  xorDecompressor->init((uint32_t *)in, len / 4, opts);
}

int RecoverDecompressor::decompress(double *output) {
  int len = getLength();
  for (int i = 0; i < len; i++) {
    if (i == 4219) {
      asm("nop");
    }
    output[i] = nextValue();
  }
  return len;
}
