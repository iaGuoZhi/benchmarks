#include "EraserCompressor.h"
#include "GorillaXORCompressor.h"
#include "ChimpXORCompressor.h"
#include "ElfXORCompressor.h"
#include "options.h"

int EraserCompressor::writeInt(int n, int len) {
  write(xorCompressor->getWriter(), n, len);
  return len;
}

int EraserCompressor::writeBit(bool bit) {
  write(xorCompressor->getWriter(), bit, 1);
  return 1;
}

void EraserCompressor::xorCompress(long vPrimeLong) {
  return xorCompressor->addValue(vPrimeLong);
}

bool EraserCompressor::useEraser() {
  return opts.getUseEraser();
}

EraserCompressor::EraserCompressor(const char *options) {
  if (options != NULL) {
    this->opts = options;
  }
}

void EraserCompressor::init(int length) {
  switch (opts.getCompressAlgo()) {
    case CombOptions::Compress_algo::Gorilla:
      xorCompressor = new GorillaXORCompressor();
      break;
    case CombOptions::Compress_algo::Chimp:
      xorCompressor = new ChimpXORCompressor();
      break;
    case CombOptions::Compress_algo::Elf:
      xorCompressor = new ElfXORCompressor();
      break;
    default:
      xorCompressor = new ElfXORCompressor();
      break;
  }
  xorCompressor->init(length, opts);
}

uint32_t* EraserCompressor::getBytes() {
  return xorCompressor->getOut();
}

void EraserCompressor::close() {
  writeInt(2, 2);
  xorCompressor->close();
}

int EraserCompressor::getSize() {
  return xorCompressor->getSize();
}

void EraserCompressor::addValue(double v) {
  DOUBLE data = {.d = v};
  long vPrimeLong;
  assert(!isnan(v));

  if (!useEraser()) {
    xorCompress(data.i);
  } else {
    if (v == 0.0) {
      writeInt(2, 2);
      vPrimeLong = data.i;
    } else {
      int *alphaAndBetaStar = getAlphaAndBetaStar(v, lastBetaStar);
      int e = ((int)(data.i >> 52)) & 0x7ff;
      int gAlpha = getFAlpha(alphaAndBetaStar[0]) + e - 1023;
      int eraseBits = 52 - gAlpha;
      long mask = 0xffffffffffffffffL << eraseBits;
      long delta = (~mask) & data.i;
      if (delta != 0 && eraseBits > 4) {
        if (alphaAndBetaStar[1] == lastBetaStar) {
          writeBit(false);
        } else {
          writeInt(alphaAndBetaStar[1] | 0x30, 6);
          lastBetaStar = alphaAndBetaStar[1];
        }
        vPrimeLong = mask & data.i;
      } else {
        writeInt(2, 2);
        vPrimeLong = data.i;
      }

      xorCompress(vPrimeLong);
    }
  }
}
