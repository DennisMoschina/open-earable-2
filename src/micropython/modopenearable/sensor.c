#include "sensor.h"

#include "SensorManager.h"
#include "SensorScheme.h"

#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#define MAX_SENSORS 128

LOG_MODULE_REGISTER(sensor, CONFIG_SENSOR_LOG_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(sensor_mp_sub, 4);

ZBUS_CHAN_DECLARE(sensor_chan);

static void sensor_data_received_callback(const struct zbus_channel *chan);
ZBUS_LISTENER_DEFINE(mp_sensor_data_listener, sensor_data_received_callback);

ZBUS_CHAN_ADD_OBS(sensor_chan, mp_sensor_data_listener, 3);

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

void register_sensor_data_callback(uint8_t sensor_id, mp_obj_t completion_handler);

/// A dictionary to hold sensor data callbacks
static mp_obj_t sensor_data_callbacks[MAX_SENSORS] = {[0 ... MAX_SENSORS-1] = MP_OBJ_NULL};

mp_obj_t openearable_on_data_received(mp_obj_t sensor_id, mp_obj_t completion_handler) {
    // Get the sensor ID
    uint8_t id = mp_obj_get_int(sensor_id);

    // make sure the completion_handler is a callable
    if (!mp_obj_is_callable(completion_handler)) {
        mp_raise_TypeError("completion_handler must be a callable");
    }

    // Register the callback for data reception
    // Assuming we have a function to register the callback
    register_sensor_data_callback(id, completion_handler);

    return mp_const_none;
}


void register_sensor_data_callback(uint8_t sensor_id, mp_obj_t completion_handler) {
    // TODO: allow multiple callbacks per sensor ID
    if (sensor_id < MAX_SENSORS) {
        sensor_data_callbacks[sensor_id] = completion_handler;
    }
}

/// Parse the sensor data and return a python dictionary
mp_obj_t parse_data(uint8_t sensor_id, uint8_t *data, size_t size) {
    LOG_DBG("Parsing data for sensor ID %d, size %zu", sensor_id, size);
    struct SensorScheme *scheme = getSensorSchemeForId(sensor_id);
    LOG_DBG("got scheme for sensor ID %d: %s", sensor_id, scheme ? scheme->name : "NULL");

    if (!scheme) {
        mp_raise_TypeError(MP_ERROR_TEXT("Invalid sensor ID"));
    }

    // Create a dictionary to hold the parsed data
    mp_obj_dict_t *parsed_data = mp_obj_new_dict(0);
    parsed_data->base.type = &mp_type_dict;

    LOG_DBG("created dict");

    size_t offset = 0;

    for (size_t i = 0; i < scheme->groupCount; i++) {
        struct SensorComponentGroup *group = &scheme->groups[i];
        mp_obj_dict_t *group_dict = mp_obj_new_dict(0);
        group_dict->base.type = &mp_type_dict;

        LOG_DBG("created group dict for group %s", group->name);

        for (size_t j = 0; j < group->componentCount; j++) {
            struct SensorComponent *component = &group->components[j];
            const char *name = component->name;
            mp_obj_t value = mp_const_none;

            switch (component->parseType) {
                case PARSE_TYPE_INT8:
                    value = mp_obj_new_int(*(int8_t *)&data[offset]);
                    offset += sizeof(int8_t);
                    break;
                case PARSE_TYPE_UINT8:
                    value = mp_obj_new_int(*(uint8_t *)&data[offset]);
                    offset += sizeof(uint8_t);
                    break;
                case PARSE_TYPE_INT16:
                    value = mp_obj_new_int((int16_t)(data[offset] | (data[offset + 1] << 8)));
                    offset += sizeof(int16_t);
                    break;
                case PARSE_TYPE_UINT16:
                    value = mp_obj_new_int((uint16_t)(data[offset] | (data[offset + 1] << 8)));
                    offset += sizeof(uint16_t);
                    break;
                case PARSE_TYPE_INT32:
                    value = mp_obj_new_int((int32_t)(
                        data[offset] |
                        (data[offset + 1] << 8) |
                        (data[offset + 2] << 16) |
                        (data[offset + 3] << 24)));
                    offset += sizeof(int32_t);
                    break;
                case PARSE_TYPE_UINT32:
                    value = mp_obj_new_int((uint32_t)(
                        data[offset] |
                        (data[offset + 1] << 8) |
                        (data[offset + 2] << 16) |
                        (data[offset + 3] << 24)));
                    offset += sizeof(uint32_t);
                    break;
                case PARSE_TYPE_FLOAT: {
                    float f;
                    memcpy(&f, &data[offset], sizeof(float));
                    value = mp_obj_new_float(f);
                    offset += sizeof(float);
                    break;
                }
                case PARSE_TYPE_DOUBLE: {
                    double d;
                    memcpy(&d, &data[offset], sizeof(double));
                    value = mp_obj_new_float(d);
                    offset += sizeof(double);
                    break;
                }
                default:
                    mp_raise_TypeError(MP_ERROR_TEXT("Unsupported parse type"));
            }

            LOG_DBG("Parsed component %s", name);

            mp_obj_dict_store(group_dict, mp_obj_new_str(name, strlen(name)), value);
            LOG_DBG("Stored component %s in group %s", name, group->name);
        }

        mp_obj_dict_store(parsed_data, mp_obj_new_str(group->name, strlen(group->name)), group_dict);
        LOG_DBG("Stored group %s in parsed data", group->name);
    }

    LOG_DBG("Finished parsing data for sensor ID %d", sensor_id);

    return MP_OBJ_FROM_PTR(parsed_data);
}

static void sensor_data_received_callback(const struct zbus_channel *chan) {
    // Get the sensor data from the channel
    const struct sensor_msg *msg = zbus_chan_const_msg(chan);
    if (!msg) {
        LOG_ERR("Received null sensor message");
        return;
    }
    if (!(msg->consumer_mask & SENSOR_CONSUMER_MP)) {
        return;
    }
    const struct sensor_data *data = &msg->data;

    // Check if we have a callback registered for this sensor ID
    if (data->id < MAX_SENSORS && sensor_data_callbacks[data->id] != MP_OBJ_NULL) {
        // Call the registered callback with the sensor data
        mp_call_function_1(sensor_data_callbacks[data->id], parse_data(data->id, data->data, data->size));
    }
}
