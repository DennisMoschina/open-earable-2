#ifndef _SENSOR_PARSER_H
#define _SENSOR_PARSER_H

#include "sensor_value.h"
#include "SensorScheme.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sensor_value parse_sensor_value(struct sensor_data sensor_data, struct SensorScheme *scheme, bool cache = true);

void clear_sensor_value_cache(struct SensorScheme *scheme);
void clear_all_sensor_value_caches(void);

#ifdef __cplusplus
}
#endif

#endif // _SENSOR_PARSER_H