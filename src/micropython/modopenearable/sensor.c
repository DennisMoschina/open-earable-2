#include "sensor.h"

#include "SensorManager.h"

mp_obj_t openearable_config_sensor(mp_obj_t sensor_id, mp_obj_t sample_rate_index, mp_obj_t storage_options) {
    // Convert the arguments to integers
    int id = mp_obj_get_int(sensor_id);
    int rate_index = mp_obj_get_int(sample_rate_index);
    int options = mp_obj_get_int(storage_options);

    // Create a sensor_config object
    struct sensor_config config;
    config.sensorId = id;
    config.sampleRateIndex = rate_index;
    config.storageOptions = options;

    // Call the C function to configure the sensor
    config_sensor(&config);

    return mp_const_none;
}