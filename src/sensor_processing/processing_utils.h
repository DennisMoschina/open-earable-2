#ifndef _PROCESSING_UTILS_H
#define _PROCESSING_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ParseType.h"
#include <inttypes.h>

float decode_as_float(enum ParseType t, const uint8_t *p);

#ifdef __cplusplus
}
#endif

#endif // _PROCESSING_UTILS_H
