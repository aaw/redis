#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "zmalloc.h"
#include "hyperloglog.h"
#include "dict.h" /* Since we're currently hijacking dictGenHashFunction */

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

void hyperloglogAdd(hyperloglog* hll, const unsigned char* value, int len) {
    int hash = dictGenHashFunction(value, len);
    int bucket = hash & ((1 << HLL_B) - 1);
    int first_one = __builtin_clz(hash) + 1;
    int rho = first_one < (HLL_HASH_BITS + 1) ? first_one : (HLL_HASH_BITS + 1);
    if (hll->contents[bucket] < rho) hll->contents[bucket] = rho;
}
