#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int comb_encode(double *in, size_t len, uint8_t **out, double error, const char* options);
int comb_decode(uint8_t *in, size_t len, double *out, double error, const char* options);

#ifdef __cplusplus
}
#endif
