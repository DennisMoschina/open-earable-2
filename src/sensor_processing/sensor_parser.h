#ifndef _SENSOR_PARSER_H
#define _SENSOR_PARSER_H

#include "sensor_value.h"
#include "SensorScheme.h"

struct sensor_value parse_sensor_value(const char *data, size_t length, struct SensorScheme *scheme, bool cache = true);

void clear_sensor_value_cache(struct SensorScheme *scheme);
void clear_all_sensor_value_caches(void);

#endif // _SENSOR_PARSER_H