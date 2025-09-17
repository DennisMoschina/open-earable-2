#include "processing_utils.h"
#include <string.h> // memcpy

float decode_as_float(enum ParseType t, const uint8_t* p) {
    switch (t) {
        case PARSE_TYPE_UINT8:  { uint8_t  v; memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_INT8:   { int8_t   v; memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_UINT16: { uint16_t v; memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_INT16:  { int16_t  v; memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_UINT32: { uint32_t v; memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_INT32:  { int32_t  v; memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_FLOAT:  { float    v; memcpy(&v, p, sizeof(v)); return v; }
        case PARSE_TYPE_DOUBLE: { double   v; memcpy(&v, p, sizeof(v)); return (float)v; }
        default: return 0.0f;
    }
}