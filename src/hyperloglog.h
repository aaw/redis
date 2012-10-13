#ifndef __HYPERLOGLOG_H
#define __HYPERLOGLOG_H
#include <stdint.h>

#define HLL_B 11
#define HLL_M (1 << HLL_B)
#define HLL_HASH_BITS (32 - HLL_B)
#define HLL_ALPHA (0.7213/(1 + 1.079/(1 << HLL_B)))

typedef struct hyperloglog {
    int8_t contents[HLL_M];
} hyperloglog;

hyperloglog *hyperloglogNew(void);
uint64_t hyperloglogCard(hyperloglog* hll);
void hyperloglogAdd(hyperloglog* hll, const unsigned char* value, int len);

#endif // __HYPERLOGLOG_H
