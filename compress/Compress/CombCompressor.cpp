#include <cstdint>
#include "Eraser.h"
#include "options.h"
#include "comb.h"

int comb_encode(double *in, size_t len, uint8_t **out, double error, const char *options) {
  Eraser compressor(options);
  compressor.init(len);
  for (int i = 0; i < len; i++) {
    compressor.addValue(in[i]);
  }
  compressor.close();
  *out = (uint8_t *)compressor.getBytes();
  return compressor.getSize();
}
