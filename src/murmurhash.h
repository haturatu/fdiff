#ifndef MURMURHASH_H
#define MURMURHASH_H
#include <string.h>
#include <stdint.h>

uint64_t murmurhash64(const void *key, int len, uint64_t seed);

#endif

