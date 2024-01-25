#pragma once
#include <cstdint>
#include <stdlib.h>
#include "BitStream/BitReader.h"
#include "BitStream/BitWriter.h"
#include "options.h"
#include "defs.h"

class AbstractCompressor {
protected:
  bool first = true;
  size_t length = 0;
  size_t size = 0;
  BitWriter writer;
  uint32_t *output;
  CombOptions opts;

  virtual void writeFirst(long value) = 0;
  virtual void compressValue(long value) = 0;

public:
  BitWriter *getWriter() { return &writer; }

  inline void init(size_t size, CombOptions opts) {
    size *= 12;
    output = (uint32_t *)malloc(size + 4);
    initBitWriter(&writer, output + 1, size / sizeof(uint32_t));
    this->opts = opts;
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
