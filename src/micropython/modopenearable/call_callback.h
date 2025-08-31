#ifndef CALL_CALLBACK_H
#define CALL_CALLBACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py/obj.h"

#include "openearable_common.h"
#include "SensorScheme.h"

void call_callback(mp_obj_t callback,
                   const struct sensor_data *data,
                   const struct SensorScheme *scheme);
#ifdef __cplusplus
}
#endif

#endif // CALL_CALLBACK_H