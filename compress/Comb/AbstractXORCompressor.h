#pragma once
#include <cstdint>
#include <stdlib.h>
#include "BitStream/BitReader.h"
#include "BitStream/BitWriter.h"
#include "options.h"

class AbstractXORCompressor {
protected:
  bool first = true;
  size_t length = 0;
  size_t size = 0;
  BitWriter writer;
  uint32_t *output;
  CombOptions::XOR_type xor_type;

  virtual void writeFirst(long value) = 0;
  virtual void compressValue(long value) = 0;

public:
  BitWriter *getWriter() { return &writer; }

  inline bool getXORType() {
    return xor_type;
  }

  inline void init(size_t size, CombOptions::XOR_type xor_type) {
    size *= 12;
    output = (uint32_t *)malloc(size + 4);
    initBitWriter(&writer, output + 1, size / sizeof(uint32_t));
    this->xor_type = xor_type;
  }

  inline void addValue(long value) {
    if (first) {
      writeFirst(value);
      first = false;
    } else {
      compressValue(value);
    }
  }

  inline void close() {
    *output = length;
    size = (flush(&writer) * 4 + 4);
  }

  inline size_t getSize() { return size; }

  inline uint32_t *getOut() { return output; }
};
