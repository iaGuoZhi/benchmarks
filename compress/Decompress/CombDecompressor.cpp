#include <cstdint>
#include "RecoverDecompressor.h"
#include "options.h"
#include "defs.h"
#include "comb.h"

int comb_decode(uint8_t *in, size_t len, double *out, double error, const char *options) {
  RecoverDecompressor decompressor(in, len, options);
  return decompressor.decompress(out);
}
