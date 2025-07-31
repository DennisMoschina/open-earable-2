#include "py/runtime.h"

mp_obj_t openearable_config_sensor(mp_obj_t sensor_id, mp_obj_t sample_rate_index, mp_obj_t storage_options);
mp_obj_t openearable_on_data_received(mp_obj_t sensor_id, mp_obj_t completion_handler);
