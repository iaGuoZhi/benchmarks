#include <cstdint>
#include "Recover.h"
#include "options.h"
#include "defs.h"
#include "comb.h"

int comb_decode(uint8_t *in, size_t len, double *out, double error, const char *options) {
  Recover recover(in, len, options);
  return recover.decompress(out);
}
