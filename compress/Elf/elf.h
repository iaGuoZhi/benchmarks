#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int elf_encode(double *in, size_t len, uint8_t **out, double error);
int elf_decode(uint8_t *in, size_t len, double *out, double error);

#ifdef __cplusplus
}
#endif
