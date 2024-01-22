#include <assert.h>
#include <cstdint>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BitStream/BitReader.h"
#include "BitStream/BitWriter.h"
#include "options.h"
#include "defs.h"
#include "comb.h"

class AbstractCombCompressor {
private:
  int lastBetaStar = __INT32_MAX__;

protected:
  virtual int writeInt(int n, int len) = 0;
  virtual int writeBit(bool bit) = 0;
  virtual void xorCompress(long vPrimeLong) = 0;
  virtual bool useEraser() = 0;
  virtual int getSize() = 0;

public:
  void addValue(double v) {
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

        delete[] alphaAndBetaStar;
      }
      xorCompress(vPrimeLong);
    }
  }
};

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

  bool getXORType() {
    return xor_type;
  }

  void init(size_t size, CombOptions::XOR_type xor_type) {
    size *= 12;
    output = (uint32_t *)malloc(size + 4);
    initBitWriter(&writer, output + 1, size / sizeof(uint32_t));
    this->xor_type = xor_type;
  }

  void addValue(long value) {
    if (first) {
      writeFirst(value);
      first = false;
    } else {
      compressValue(value);
    }
  }

  void close() {
    *output = length;
    size = (flush(&writer) * 4 + 4);
  }

  size_t getSize() { return size; }

  uint32_t *getOut() { return output; }
};

class GorillaXORCompressor : public AbstractXORCompressor {
private:
  uint64_t prevLeading = -1L;
  uint64_t prevTrailing = 0;
  uint64_t prevVal = 0;
  uint64_t leading, trailing;
  uint64_t mask;
  double sum;

  void writeFirst(long value) {
    writeLong(&writer, value, 64);
    length = 1;
    prevVal = value;
  }

  void compressValue(long value) {
    uint64_t vDelta = value ^ prevVal;
    prevVal = value;
    if (vDelta == 0) {
      write(&writer, 0, 1);
    } else {
      leading = __builtin_clzl(vDelta);
      trailing = __builtin_ctzl(vDelta);

      leading = (leading >= 32) ? 31 : leading;
      uint64_t l;

      if (prevLeading != -1L && leading >= prevLeading &&
          trailing >= prevTrailing) {
        write(&writer, 2, 2);
        l = 64 - prevLeading - prevTrailing;
      } else {
        prevLeading = leading;
        prevTrailing = trailing;
        l = 64 - leading - trailing;

        write(&writer, 3, 2);
        write(&writer, leading, 5);
        write(&writer, l - 1, 6);
      }

      if (l <= 32) {
        write(&writer, vDelta >> prevTrailing, l);
      } else {
        write(&writer, vDelta >> 32, 32 - prevLeading);
        write(&writer, vDelta >> prevTrailing, 32 - prevTrailing);
      }
    }
    length++;
  }
};

static const uint16_t leadingRep[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5,
      6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

  static const uint16_t leadingRnd[] = {
      0,  0,  0,  0,  0,  0,  0,  0,  8,  8,  8,  8,  12, 12, 12, 12,
      16, 16, 18, 18, 20, 20, 22, 22, 24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24};

class ChimpXORCompressor : public AbstractXORCompressor {
private:
  const int32_t PREVIOUS_VALUES = 128;
  int32_t storedLeadingZeros = INT32_MAX;
  int32_t index = 0, current = 0;
  int32_t previousValues = PREVIOUS_VALUES;
  int32_t previousValuesLog2 = 31 - __builtin_clz(PREVIOUS_VALUES);
  int32_t threshold = 6 + previousValuesLog2;
  int32_t setLsb = (1 << (threshold + 1)) - 1;
  int32_t *indices;
  int64_t *storedValues;
  int32_t flagZeroSize = 1 + 1 + previousValuesLog2;
  int32_t flagOneSize = 1 + 1 + previousValuesLog2 + 3 + 6;

  void writeFirst(long value) {
    writeLong(&writer, value, 64);
    storedValues[current] = value;
    indices[((int)value) & setLsb] = index;
    length = 1;
  }

  void compressValue(long value) {
    int32_t key = (int)value & setLsb;
    int64_t delta;
    int32_t previousIndex;
    int32_t trailingZeros = 0;
    int32_t currIndex = indices[key];
    if ((index - currIndex) < previousValues) {
      delta = value ^ storedValues[currIndex % previousValues];
      trailingZeros = __builtin_ctzl(delta);
      if (trailingZeros > threshold) {
        previousIndex = currIndex % previousValues;
      } else {
        previousIndex = index % previousValues;
        delta = storedValues[previousIndex] ^ value;
      }
    } else {
      previousIndex = index % previousValues;
      delta = storedValues[previousIndex] ^ value;
    }

    if (delta == 0) {
      // {0_0_previousIndex}
      write(&writer, previousIndex, flagZeroSize);
      storedLeadingZeros = 65;
    } else {
      int32_t leadingZeros = leadingRnd[__builtin_clzl(delta)];

      if (trailingZeros > threshold) {
        int32_t significantBits = 64 - leadingZeros - trailingZeros;
        // {0_1_previousIndex_leadingZeros_significantBits}
        write(&writer,
              ((previousValues + previousIndex) << 9) |
                  (leadingRep[leadingZeros] << 6) | significantBits,
              flagOneSize);

        writeLong(&writer, delta >> trailingZeros, significantBits);
        storedLeadingZeros = 65;
      } else if (leadingZeros == storedLeadingZeros) {
        // {10_delta}
        write(&writer, 2, 2);
        int32_t significantBits = 64 - leadingZeros;
        writeLong(&writer, delta, significantBits);
      } else {
        // {11_leadingZeros_delta}
        storedLeadingZeros = leadingZeros;
        int significantBits = 64 - leadingZeros;
        write(&writer, (0x3 << 3) | leadingRep[leadingZeros], 5);
        writeLong(&writer, delta, significantBits);
      }
    }
    current = (current + 1) % previousValues;
    storedValues[current] = value;
    index++;
    indices[key] = index;
    length++;
  }

public:
  ChimpXORCompressor() {
    indices = (int32_t *)calloc(sizeof(int32_t), (1 << (threshold + 1)));
    storedValues = (int64_t *)calloc(sizeof(int64_t), PREVIOUS_VALUES);
  }

  ~ChimpXORCompressor() {
    free(indices);
    free(storedValues);
  }
};

static const short leadingRepresentation[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5,
    6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static const short leadingRound[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  8,  8,  8,  8,  12, 12, 12, 12,
    16, 16, 18, 18, 20, 20, 22, 22, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24};

class ElfXORCompressor : public AbstractXORCompressor {
private:
  int storedLeadingZeros = __INT32_MAX__;
  int storedTrailingZeros = __INT32_MAX__;
  uint64_t storedVal = 0;

  void writeFirst(long value) {
    storedVal = value;
    length = 1;
    int trailingZeros = __builtin_ctzl(value);
    write(&writer, trailingZeros, 7);
    if (value != 0) {
      writeLong(&writer, storedVal >> (trailingZeros + 1), 63 - trailingZeros);
    }
  }

  void compressValue(long value) {
    uint64_t _xor = storedVal ^ value;
    if (_xor == 0) {
      write(&writer, 1, 2);
    } else {
      int leadingZeros = leadingRound[__builtin_clzl(_xor)];
      int trailingZeros = __builtin_ctzl(_xor);

      if (leadingZeros == storedLeadingZeros &&
          trailingZeros >= storedTrailingZeros) {
        int centerBits = 64 - storedLeadingZeros - storedTrailingZeros;
        int len = 2 + centerBits;
        if (len > 64) {
          write(&writer, 0, 2);
          writeLong(&writer, _xor >> storedTrailingZeros, centerBits);
        } else {
          writeLong(&writer, _xor >> storedTrailingZeros, len);
        }
      } else {
        storedLeadingZeros = leadingZeros;
        storedTrailingZeros = trailingZeros;
        int centerBits = 64 - storedLeadingZeros - storedTrailingZeros;

        if (centerBits <= 16) {
          write(
              &writer,
              (((0x2 << 3) | leadingRepresentation[storedLeadingZeros]) << 4) |
                  (centerBits & 0xf),
              9);
          writeLong(&writer, _xor >> (storedTrailingZeros + 1), centerBits - 1);
        } else {
          write(
              &writer,
              (((0x3 << 3) | leadingRepresentation[storedLeadingZeros]) << 6) |
                  (centerBits & 0x3f),
              11);
          writeLong(&writer, _xor >> (storedTrailingZeros + 1), centerBits - 1);
        }
      }
      storedVal = value;
    }
    length++;
  }
};

class CombCompressor : public AbstractCombCompressor {
private:
  CombOptions opts;
  AbstractXORCompressor *xorCompressor;

protected:
  int writeInt(int n, int len) override {
    write(xorCompressor->getWriter(), n, len);
    return len;
  }

  int writeBit(bool bit) override {
    write(xorCompressor->getWriter(), bit, 1);
    return 1;
  }

  void xorCompress(long vPrimeLong) override {
    xorCompressor->addValue(vPrimeLong);
  }

  bool useEraser() override { return opts.getUseEraser(); }

public:
  CombCompressor(const char *options) {
    if (options != NULL) {
      opts = CombOptions(options);
    }
  }

  void init(int length) {
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
    xorCompressor->init(length, opts.getXORType());
  }
  uint32_t *getBytes() { return xorCompressor->getOut(); }

  void close() {
    writeInt(2, 2);
    xorCompressor->close();
  }

  int getSize() override { return xorCompressor->getSize(); }
};

int comb_encode(double *in, size_t len, uint8_t **out, double error, const char *options) {
  CombCompressor compressor(options);
  compressor.init(len);
  for (int i = 0; i < len; i++) {
    compressor.addValue(in[i]);
  }
  compressor.close();
  *out = (uint8_t *)compressor.getBytes();
  return compressor.getSize();
}
