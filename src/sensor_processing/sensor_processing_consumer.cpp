#include "sensor_processing_stage.h"
#include "sensor_value.h"
#include "sensor_parser.h"

#include <zephyr/zbus/zbus.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_processing_consumer, LOG_LEVEL_DBG);

ZBUS_SUBSCRIBER_DEFINE(sensor_processing_sub, 4);
ZBUS_CHAN_DECLARE(sensor_chan);

static void sensor_data_received_callback(const struct zbus_channel *chan);
ZBUS_LISTENER_DEFINE(sensor_processing_listener, sensor_data_received_callback);
ZBUS_CHAN_ADD_OBS(sensor_chan, sensor_processing_listener, 3);

static SensorProcessingStage *processing_pipeline[256] = {nullptr};

void set_processing_pipeline(SensorProcessingStage *stage, uint8_t sensor_id) {
    if (sensor_id < 256) {
        if (processing_pipeline[sensor_id]) {
            LOG_DBG("Replacing existing processing pipeline for sensor ID %d", sensor_id);
            delete processing_pipeline[sensor_id];
        }
        processing_pipeline[sensor_id] = stage;
    }
}

void remove_processing_pipeline(uint8_t sensor_id) {
    if (sensor_id < 256) {
        delete processing_pipeline[sensor_id];
        processing_pipeline[sensor_id] = nullptr;
    }
}

static void sensor_data_received_callback(const struct zbus_channel *chan) {
    const struct sensor_msg *msg = zbus_chan_const_msg(chan);
    if (!msg) {
        LOG_ERR("Received null sensor_msg");
        return;
    }
    if (!(msg->consumer_mask & SENSOR_CONSUMER_PROCESSING)) {
        return;
    }
    const struct sensor_data *data = &msg->data;

    SensorProcessingStage *pipeline = processing_pipeline[data->id];
    if (!pipeline) {
        LOG_WRN("No processing pipeline set for sensor ID %d", data->id);
        return;
    }
}