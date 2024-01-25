#include <cmath>
#include "Eraser.h"
#include "GorillaXORCompressor.h"
#include "ChimpXORCompressor.h"
#include "ElfXORCompressor.h"
#include "TimestampCompressor.h"
#include "options.h"

int Eraser::writeInt(int n, int len) {
  write(compressor->getWriter(), n, len);
  return len;
}

int Eraser::writeBit(bool bit) {
  write(compressor->getWriter(), bit, 1);
  return 1;
}

void Eraser::xorCompress(long vPrimeLong) {
  return compressor->addValue(vPrimeLong);
}

bool Eraser::useEraser() {
  return opts.getUseEraser();
}

Eraser::Eraser(const char *options) {
  if (options != NULL) {
    this->opts = CombOptions(options);
  }
}

void Eraser::init(int length) {
  switch (opts.getCompressAlgo()) {
    case CombOptions::Compress_algo::Gorilla:
      compressor = new GorillaXORCompressor();
      break;
    case CombOptions::Compress_algo::Chimp:
      compressor = new ChimpXORCompressor();
      break;
    case CombOptions::Compress_algo::Elf:
      compressor = new ElfXORCompressor();
      break;
    case CombOptions::Compress_algo::Timestamp:
      compressor = new TimestampCompressor();
      break;
    default:
      compressor = new GorillaXORCompressor();
      break;
  }
  compressor->init(length, opts);
}

uint32_t* Eraser::getBytes() {
  return compressor->getOut();
}

void Eraser::close() {
  writeInt(2, 2);
  compressor->close();
}

int Eraser::getSize() {
  return compressor->getSize();
}

void Eraser::addValue(double v) {
  DOUBLE data = {.d = v};
  long vPrimeLong;

  if (!useEraser()) {
    xorCompress(data.i);
  } else {
    assert(!std::isnan(v));
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
      delete[] alphaAndBetaStar;

      xorCompress(vPrimeLong);
    }
  }
}

Eraser::~Eraser() {
  delete compressor;
}
