#include "modsensor.h"
#include "SensorManager.h"
#include "SensorScheme.h"
#include "ParseType.h"

#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#define MAX_SENSORS 128

#define MAX_SENSOR_QUEUE_SIZE 8

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(sensor_mp_sub, 4);
ZBUS_CHAN_DECLARE(sensor_chan);

static void sensor_data_received_callback(const struct zbus_channel *chan);
ZBUS_LISTENER_DEFINE(mp_sensor_data_listener, sensor_data_received_callback);
// ZBUS_CHAN_ADD_OBS(sensor_chan, mp_sensor_data_listener, 3);

static mp_obj_t sensor_data_callbacks[MAX_SENSORS] = {[0 ... MAX_SENSORS - 1] = MP_OBJ_NULL};

K_THREAD_STACK_DEFINE(mp_sensor_work_stack, 1024);
K_MSGQ_DEFINE(mp_sensor_work_msg_q, sizeof(struct sensor_data), MAX_SENSOR_QUEUE_SIZE, 4);

static struct k_thread mp_sensor_work_thread;
static k_tid_t mp_sensor_work_tid;

struct sensor_data sensor_data;

static struct k_mutex sensor_data_mutex;

mp_obj_t openearable_config_sensor(mp_obj_t sensor_id, mp_obj_t sample_rate_index, mp_obj_t storage_options) {
    mp_obj_t test_dict = mp_obj_new_dict(0);
    mp_obj_dict_store(test_dict, mp_obj_new_str("sensorId", 8), sensor_id);
    mp_obj_dict_store(test_dict, mp_obj_new_str("sampleRateIndex", 16), sample_rate_index);
    mp_obj_dict_store(test_dict, mp_obj_new_str("storageOptions", 15), storage_options);

    // print the dictionary for debugging
    mp_obj_print_helper(&mp_plat_print, test_dict, PRINT_REPR);

    struct sensor_config config = {
        .sensorId = mp_obj_get_int(sensor_id),
        .sampleRateIndex = mp_obj_get_int(sample_rate_index),
        .storageOptions = mp_obj_get_int(storage_options),
    };
    config_sensor(&config);
    return mp_const_none;
}


void register_sensor_data_callback(uint8_t sensor_id, mp_obj_t completion_handler);

mp_obj_t openearable_on_data_received(mp_obj_t sensor_id, mp_obj_t completion_handler) {
    uint8_t id = mp_obj_get_int(sensor_id);
    if (!mp_obj_is_callable(completion_handler)) {
        mp_raise_TypeError(MP_ERROR_TEXT("completion_handler must be a callable"));
    }
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
mp_obj_t parse_data(uint8_t sensor_id, uint64_t timestamp, uint8_t *data, size_t size) {
    struct SensorScheme *scheme = getSensorSchemeForId(sensor_id);
    if (!scheme) {
        mp_raise_TypeError(MP_ERROR_TEXT("Invalid sensor ID"));
    }

    mp_obj_t parsed_data = mp_obj_new_dict(0);
    size_t offset = 0;

    mp_obj_t groups_dict = mp_obj_new_dict(0);

    for (size_t i = 0; i < scheme->groupCount; i++) {
        struct SensorComponentGroup *group = &scheme->groups[i];
        mp_obj_t group_dict = mp_obj_new_dict(0);

        for (size_t j = 0; j < group->componentCount; j++) {
            struct SensorComponent *component = &group->components[j];
            const char *name = component->name;
            mp_obj_t value = mp_const_none;

            size_t bytes_needed = parseTypeSizes[component->parseType];

            if (offset + bytes_needed > size) {
                mp_raise_ValueError(MP_ERROR_TEXT("Sensor data buffer too small"));
            }

            switch (component->parseType) {
                case PARSE_TYPE_INT8:
                    value = mp_obj_new_int(*(int8_t *)&data[offset]); break;
                case PARSE_TYPE_UINT8:
                    value = mp_obj_new_int(*(uint8_t *)&data[offset]); break;
                case PARSE_TYPE_INT16:
                    value = mp_obj_new_int((int16_t)(data[offset] | (data[offset + 1] << 8))); break;
                case PARSE_TYPE_UINT16:
                    value = mp_obj_new_int((uint16_t)(data[offset] | (data[offset + 1] << 8))); break;
                case PARSE_TYPE_INT32:
                    value = mp_obj_new_int((int32_t)(
                        data[offset] |
                        (data[offset + 1] << 8) |
                        (data[offset + 2] << 16) |
                        (data[offset + 3] << 24))); break;
                case PARSE_TYPE_UINT32:
                    value = mp_obj_new_int((uint32_t)(
                        data[offset] |
                        (data[offset + 1] << 8) |
                        (data[offset + 2] << 16) |
                        (data[offset + 3] << 24))); break;
                case PARSE_TYPE_FLOAT: {
                    float f;
                    memcpy(&f, &data[offset], sizeof(float));
                    value = mp_obj_new_float(f);
                    break;
                }
                case PARSE_TYPE_DOUBLE: {
                    double d;
                    memcpy(&d, &data[offset], sizeof(double));
                    value = mp_obj_new_float(d);
                    break;
                }
                default: break;
            }

            offset += bytes_needed;
            mp_obj_dict_store(MP_OBJ_TO_PTR(group_dict), mp_obj_new_str(name, strlen(name)), value);
        }

        mp_obj_dict_store(MP_OBJ_TO_PTR(groups_dict), mp_obj_new_str(group->name, strlen(group->name)), group_dict);
    }

    mp_obj_dict_store(MP_OBJ_TO_PTR(parsed_data), mp_obj_new_str("groups", 6), groups_dict);
    mp_obj_dict_store(MP_OBJ_TO_PTR(parsed_data), mp_obj_new_str("sensorId", 8), mp_obj_new_int(sensor_id));
    mp_obj_dict_store(MP_OBJ_TO_PTR(parsed_data), mp_obj_new_str("timestamp", 9), mp_obj_new_int(timestamp));
    return parsed_data;
}

static void sensor_data_received_callback(const struct zbus_channel *chan) {
    const struct sensor_msg *msg = zbus_chan_const_msg(chan);
    if (!msg || !(msg->consumer_mask & SENSOR_CONSUMER_PROCESSING)) {
        return;
    }

    int ret = k_msgq_put(&mp_sensor_work_msg_q, &msg->data, K_NO_WAIT);

    if (ret) {
        LOG_ERR("MicroPython sensor queue full, dropping sample");
    }
}

static mp_obj_t mp_sensor_parser(mp_obj_t unused) {
    struct sensor_data local;

    k_mutex_lock(&sensor_data_mutex, K_FOREVER);
    memcpy(&local, &sensor_data, sizeof(local));
    k_mutex_unlock(&sensor_data_mutex);

    mp_obj_t cb = sensor_data_callbacks[local.id];
    if (cb == MP_OBJ_NULL) {
        return mp_const_none;
    }

    mp_obj_t parsed = parse_data(local.id, local.time, local.data, local.size);
    if (parsed != MP_OBJ_NULL) {
        mp_call_function_1(cb, parsed);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sensor_parser_fun_obj, mp_sensor_parser);

static void sensor_worker_loop(void) {
    int ret;

    while (1) {
        struct sensor_data data;
        ret = k_msgq_get(&mp_sensor_work_msg_q, &data, K_FOREVER);
        LOG_DBG("Received sensor data for ID %d, size %zu", data.id, data.size);
        if (ret != 0) {
            LOG_ERR("Failed to get sensor data from queue: %d", ret);
            continue;
        }
        if (data.id >= MAX_SENSORS) {
            LOG_ERR("Invalid sensor ID: %d", data.id);
            continue;
        }

        ret = k_mutex_lock(&sensor_data_mutex, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("Failed to lock sensor data mutex: %d", ret);
            continue;
        }
        sensor_data = data;
        ret = k_mutex_unlock(&sensor_data_mutex);
        if (ret != 0) {
            LOG_ERR("Failed to unlock sensor data mutex: %d", ret);
            continue;
        }

        mp_sched_schedule((mp_obj_t)&mp_sensor_parser_fun_obj, mp_const_none);
    }
}


int init_mp_sensor(void) {
    int ret;

    // ret = k_mutex_init(&sensor_data_mutex);
    // if (ret != 0) {
    //     LOG_ERR("Failed to initialize sensor data mutex: %d", ret);
    //     return ret;
    // }

    // mp_sensor_work_tid = k_thread_create(
    //     &mp_sensor_work_thread, mp_sensor_work_stack,
    //     K_THREAD_STACK_SIZEOF(mp_sensor_work_stack), (k_thread_entry_t)sensor_worker_loop,
    //     NULL, NULL, NULL, K_PRIO_PREEMPT(6), 0, K_NO_WAIT);
    // ret = k_thread_name_set(mp_sensor_work_tid, "MP_SENSOR_WORK");
    // if (ret) {
    //     LOG_ERR("Failed to create MP sensor worker thread: %d", ret);
    //     return ret;
    // }

    return 0;
}
