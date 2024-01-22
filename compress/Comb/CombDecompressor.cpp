#include <assert.h>
#include <cstdint>
#include <math.h>
#include <stdio.h>

#include "BitStream/BitReader.h"
#include "BitStream/BitWriter.h"
#include "options.h"
#include "defs.h"
#include "comb.h"

class AbstractCombDecompressor {
private:
  int lastBetaStar = __INT32_MAX__;

  double nextValue() {
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

  double recoverVByBetaStar() {
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

protected:
  virtual double xorDecompress() = 0;
  virtual int readInt(int len) = 0;
  virtual int getLength() = 0;
  virtual bool useEraser() = 0;

public:
  int decompress(double *output) {
    int len = getLength();
    for (int i = 0; i < len; i++) {
      if (i == 4219) {
        asm("nop");
      }
      output[i] = nextValue();
    }
    return len;
  }
};

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

class GorillaXORDecompressor : public AbstractXORDecompressor {
private:
  DOUBLE prevVal = {.i = 0}, value = {.i = 0};
  uint64_t leading, meaningful, delta;

  double firstValue() {
    prevVal.i = readLong(&reader, 64);
    return prevVal.d;
  }

  uint64_t read_delta(BitReader *reader, uint64_t leading, uint64_t meaningful) {
    uint64_t trailing = 64 - leading - meaningful;
    uint64_t delta = readLong(reader, meaningful);
    return delta << trailing;
  }

  double nextValue() {
    switch (peek(&reader, 2)) {
    case 0:
    case 1:
      forward(&reader, 1);
      value.d = prevVal.d;
      break;
    case 2:
      forward(&reader, 2);
      delta = read_delta(&reader, leading, meaningful);
      value.i = (prevVal.i) ^ delta;
      break;
    case 3:
      forward(&reader, 2);
      leading = read(&reader, 5);
      meaningful = read(&reader, 6) + 1;
      delta = read_delta(&reader, leading, meaningful);
      value.i = (prevVal.i) ^ delta;
      break;
    default:
      break;
    }
    prevVal.i = value.i;
    return value.d;
  }
};

static const int16_t leadingRep[] = {0, 8, 12, 16, 18, 20, 22, 24};

class ChimpXORDecompressor : public AbstractXORDecompressor {
private:
  DOUBLE prevVal = {.i = 0}, value = {.i = 0};
  const int PREVIOUS_VALUES = 128;
  int32_t previousValues = PREVIOUS_VALUES;
  int32_t previousValuesLog2 = 31 - __builtin_clz(PREVIOUS_VALUES);
  int32_t storedLeadingZeros = INT32_MAX;
  int32_t storedTrailingZeros = 0;
  int32_t initialFill = previousValuesLog2 + 9;
  int64_t *storedValues;
  uint32_t tmp, fill, index, significantBits;
  int32_t i = 0;

  int64_t delta;

  double firstValue() {
    prevVal.i = readLong(&reader, 64);
    storedValues[i++] = prevVal.i;
    return prevVal.d;
  }

  double nextValue() {
    switch (read(&reader, 2)) {
    case 3:
      tmp = read(&reader, 3);
      storedLeadingZeros = leadingRep[tmp];
      delta = readLong(&reader, 64 - storedLeadingZeros);
      value.i = prevVal.i ^ delta;
      storedValues[i % previousValues] = value.i;
      break;
    case 2:
      delta = readLong(&reader, 64 - storedLeadingZeros);
      value.i = prevVal.i ^ delta;
      storedValues[i % previousValues] = value.i;
      break;
    case 1:
      fill = initialFill;
      tmp = read(&reader, fill);
      fill -= previousValuesLog2;
      index = (tmp >> fill) & ((1 << previousValuesLog2) - 1);
      fill -= 3;
      storedLeadingZeros = leadingRep[(tmp >> fill) & 0x7];
      fill -= 6;
      significantBits = (tmp >> fill) & 0x3f;
      if (significantBits == 0) {
        significantBits = 64;
      }
      storedTrailingZeros = 64 - significantBits - storedLeadingZeros;
      delta = readLong(&reader, significantBits);
      value.i = storedValues[index] ^ (delta << storedTrailingZeros);
      storedValues[i % previousValues] = value.i;
      break;
    default:
      value.i = storedValues[readLong(&reader, previousValuesLog2)];
      storedValues[i % previousValues] = value.i;
      break;
    }
    prevVal.i = value.i;
    i++;
    return value.d;
  }

public:
  ChimpXORDecompressor() {
    storedValues = (int64_t *)calloc(sizeof(int64_t), previousValues);
  }

  ~ChimpXORDecompressor() { free(storedValues); }
};

static const short leadingRepresentation[] = {0, 8, 12, 16, 18, 20, 22, 24};

class ElfXORDecompressor : public AbstractXORDecompressor {
private:
  DOUBLE storedVal = {.i = 0};
  int storedLeadingZeros = __INT32_MAX__;
  int storedTrailingZeros = __INT32_MAX__;

  double firstValue() {
    int trailingZeros = read(&reader, 7);
    if (trailingZeros < 64) {
      storedVal.i = ((readLong(&reader, 63 - trailingZeros) << 1) + 1)
                    << trailingZeros;
    } else {
      storedVal.i = 0;
    }
    return storedVal.d;
  }

  double nextValue() {
    long value;
    int centerBits;
    uint32_t leadAndCenter;
    int flag = read(&reader, 2);
    switch (flag) {
    case 3:
      leadAndCenter = read(&reader, 9);
      storedLeadingZeros = leadingRepresentation[leadAndCenter >> 6];
      centerBits = leadAndCenter & 0x3f;
      if (centerBits == 0) {
        centerBits = 64;
      }
      storedTrailingZeros = 64 - storedLeadingZeros - centerBits;
      value = ((readLong(&reader, centerBits - 1) << 1) + 1)
              << storedTrailingZeros;
      value = storedVal.i ^ value;
      storedVal.i = value;
      break;
    case 2:
      leadAndCenter = read(&reader, 7);
      storedLeadingZeros = leadingRepresentation[leadAndCenter >> 4];
      centerBits = leadAndCenter & 0xf;
      if (centerBits == 0) {
        centerBits = 16;
      }
      storedTrailingZeros = 64 - storedLeadingZeros - centerBits;
      value = ((readLong(&reader, centerBits - 1) << 1) + 1)
              << storedTrailingZeros;
      value = storedVal.i ^ value;
      storedVal.i = value;
      break;
    case 1:
      break;
    default:
      centerBits = 64 - storedLeadingZeros - storedTrailingZeros;
      value = readLong(&reader, centerBits) << storedTrailingZeros;
      value = storedVal.i ^ value;
      storedVal.i = value;
      break;
    }
    return storedVal.d;
  }
};

class CombDecompressor : public AbstractCombDecompressor {
private:
  CombOptions opts;
  ElfXORDecompressor elfXORDecompressor;
  AbstractXORDecompressor *xorDecompressor;

protected:
  double xorDecompress() override { return xorDecompressor->readValue(); }

  int readInt(int len) override {
    return read(xorDecompressor->getReader(), len);
  }

  int getLength() override { return xorDecompressor->length; }

  bool useEraser() override { return opts.getUseEraser(); }

public:
  CombDecompressor(uint8_t *in, size_t len, const char *options) {
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
    xorDecompressor->init((uint32_t *)in, len / 4);
  }
};

int comb_decode(uint8_t *in, size_t len, double *out, double error, const char *options) {
  CombDecompressor decompressor(in, len, options);
  return decompressor.decompress(out);
}
