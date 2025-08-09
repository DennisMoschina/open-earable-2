#ifndef _SENSOR_VALUE_H
#define _SENSOR_VALUE_H

#include "ParseType.h"

#include <stddef.h>
#include <stdint.h>

typedef union {
    int8_t    i8;
    uint8_t   u8;
    int16_t   i16;
    uint16_t  u16;
    int32_t   i32;
    uint32_t  u32;
    float     f32;
    double    f64;
} sv_value_t;

typedef struct sensor_value_component {
    sv_value_t value;
    enum ParseType parse_type;
    const char *name;
    const char *unit;
} sensor_value_component_t;

typedef struct sensor_value_group {
    size_t component_count;
    sensor_value_component_t *components;
    const char *name;
} sensor_value_group_t;

typedef struct sensor_value {
    size_t group_count;
    sensor_value_group_t *groups;
    const char *name;
    uint64_t timestamp;
} sensor_value_t;

#endif // _SENSOR_VALUE_H