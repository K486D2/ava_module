#ifndef TIMEDEF_H
#define TIMEDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typedef.h"

#define NANO_PER_SEC      1000000000ULL // 10^9
#define MICRO_PER_SEC     1000000ULL    // 10^6
#define MILLI_PER_SEC     1000ULL       // 10^3

#define WIN_TO_UNIX_EPOCH 116444736000000000ULL

#ifdef __cplusplus
}
#endif

#endif // !TIMEDEF_H
