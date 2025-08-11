#include "murmurhash.h"

uint64_t murmurhash64(const void *key, int len, uint64_t seed) {
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;
    uint64_t h = seed ^ (len * m);
    const uint8_t *data = (const uint8_t *)key;
    const uint8_t *end = data + len;

    while (data + 8 <= end) {
        uint64_t k;
        memcpy(&k, data, sizeof(k));
        data += 8;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }

    uint64_t remaining = 0;
    switch (end - data) {
        case 7: remaining ^= (uint64_t)data[6] << 48;
        case 6: remaining ^= (uint64_t)data[5] << 40;
        case 5: remaining ^= (uint64_t)data[4] << 32;
        case 4: remaining ^= (uint64_t)data[3] << 24;
        case 3: remaining ^= (uint64_t)data[2] << 16;
        case 2: remaining ^= (uint64_t)data[1] << 8;
        case 1: remaining ^= (uint64_t)data[0];
                h ^= remaining;
                h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}
