#pragma once
#include <cstdint>
#include <stdlib.h>
#include "BitStream/BitReader.h"
#include "BitStream/BitWriter.h"
#include "options.h"
#include "defs.h"

class AbstractXORDecompressor {
protected:
  bool first = true;
  CombOptions::XOR_type xor_type;
  BitReader reader;

  virtual double firstValue() = 0;
  virtual double nextValue() = 0;

public:
  size_t length = 0;
  void init(uint32_t *in, size_t len) {
    initBitReader(&reader, in + 1, len - 1);
    length = in[0];
  }

  BitReader *getReader() { return &reader; }

  double readValue() {
    if (first) {
      first = false;
      return firstValue();
    } else {
      return nextValue();
    }
  }
};
