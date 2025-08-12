#include "sensor_processing_stage.h"
#include "sensor_value.h"
#include "sensor_parser.h"

#include "sample_rate_extractor.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_processing_consumer, LOG_LEVEL_WRN);

ZBUS_SUBSCRIBER_DEFINE(sensor_processing_sub, 16);
ZBUS_CHAN_DECLARE(sensor_chan);

ZBUS_CHAN_ADD_OBS(sensor_chan, sensor_processing_sub, 3);

#define PROC_STACK_SIZE  3072
#define PROC_THREAD_PRIO 5

K_THREAD_STACK_DEFINE(proc_stack, PROC_STACK_SIZE);
static struct k_thread proc_thread;

static SensorProcessingStage *processing_pipeline[256];

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

/* Dedicated consumer that blocks on the subscriber queue */
static void processing_thread(void *a, void *b, void *c)
{
    const struct zbus_channel *chan;

    while (true) {
        /* Wait until a message for our subscriber is available */
        int err = zbus_sub_wait(&sensor_processing_sub, &chan, K_FOREVER);
        if (err) {
            LOG_WRN("zbus_sub_wait err=%d", err);
            continue;
        }

        /* Copy the message atomically out of the channel */
        struct sensor_msg msg;
        err = zbus_chan_read(chan, &msg, K_NO_WAIT);
        if (err) {
            LOG_WRN("zbus_chan_read err=%d", err);
            continue;
        }

        if (!(msg.consumer_mask & SENSOR_CONSUMER_PROCESSING)) {
            continue;
        }

        SensorProcessingStage *pipeline = processing_pipeline[msg.data.id];
        if (!pipeline) {
            LOG_WRN("No processing pipeline set for sensor ID %d", msg.data.id);
            continue;
        }

        struct SensorScheme *scheme = getSensorSchemeForId(msg.data.id);
        if (!scheme) {
            LOG_WRN("No sensor scheme found for sensor ID %d", msg.data.id);
            continue;
        }

        sensor_value_t input_value = parse_sensor_value(msg.data, scheme);

        // Process the input value through the pipeline
        pipeline->input(input_value, 0);
    }
}

/* Bring up the dedicated thread */
int sensor_processing_consumer_init(void)
{
    for (size_t i = 0; i < 256; ++i) {
        processing_pipeline[i] = nullptr;
    }

    k_thread_create(&proc_thread, proc_stack, K_THREAD_STACK_SIZEOF(proc_stack),
                    processing_thread, NULL, NULL, NULL,
                    PROC_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&proc_thread, "sensor_proc");
    return 0;
}