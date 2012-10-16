#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "zmalloc.h"
#include "hyperloglog.h"

hyperloglog *hyperloglogNew(void) {
    hyperloglog *hll = zmalloc(sizeof(hyperloglog));
    memset(hll->contents, 0, sizeof(hll->contents));
    return hll; 
}

uint64_t hyperloglogCard(hyperloglog* hll) {
    const double upper_bound = (double)(1ULL << 32);
    double sum = 0;
    int non_zeros = 0;
    double estimate;
    int i, exponent;
    
    for (i = 0; i < HLL_M; i++) {
        exponent = hll->contents[i];
        sum += pow(2, -exponent);
        if (exponent > 0) non_zeros++;
    }
    estimate = HLL_ALPHA * HLL_M * HLL_M / (sum + HLL_M - non_zeros);
    
    if (estimate <= 2.5 * HLL_M) {
        if (non_zeros == HLL_M) {
            return round(estimate);
        }
        else {
            return round(HLL_M * log(((double)HLL_M) / (HLL_M - non_zeros)));
        }
    }
    else if (estimate <= upper_bound / 30.0) {
        return round(estimate);
    }
    else {
        return round(-upper_bound * log(1 - estimate/upper_bound));
    }
}

static inline uint32_t rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

static int murmurhash3(const unsigned char* value, int len) {
    /* Algorithm & source code by Austin Appleby, released to public 
       domain. This implementation adopted from MurmurHash3_x86_32.
       Source at http://murmurhash.googlepages.com */
  
    int i, k1;
    const uint8_t * data = (const uint8_t*)value;
    const int nblocks = len / 4;

    /* Start with an arbitrary, fixed seed */
    uint32_t h1 = 0xa39cf4b1;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);
    const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

    for(i = -nblocks; i; i++) {
        k1 = blocks[i];

        k1 *= c1;
        k1 = rotl32(k1,15);
        k1 *= c2;
    
        h1 ^= k1;
        h1 = rotl32(h1,13); 
        h1 = h1*5+0xe6546b64;
    }

    k1 = 0;

    switch(len & 3) {
      case 3: k1 ^= tail[2] << 16;
      case 2: k1 ^= tail[1] << 8;
      case 1: k1 ^= tail[0];
        k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
    };

    h1 ^= len;

    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

void hyperloglogAdd(hyperloglog* hll, const unsigned char* value, int len) {
    int hash = murmurhash3(value, len);
    int bucket = hash & (HLL_M - 1);
    int first_one = __builtin_clz(hash) + 1;
    int rho = first_one < (HLL_HASH_BITS + 1) ? first_one : (HLL_HASH_BITS + 1);
    if (hll->contents[bucket] < rho) hll->contents[bucket] = rho;
}
