#include "sensor_processing_stage.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_processing_stage, LOG_LEVEL_DBG);

SensorProcessingStage::SensorProcessingStage(size_t input_size, sensor_processing_stage_t *children, size_t child_count)
    : input_size(input_size), children(children), child_count(child_count) {
    input_buffer = new sensor_value_t[input_size];
    input_state = new bool[input_size];
    memset(input_state, false, input_size * sizeof(bool));
}

SensorProcessingStage::~SensorProcessingStage() {
    delete[] input_buffer;
    delete[] input_state;
}

void SensorProcessingStage::input(sensor_value_t value, size_t index) {
    if (index >= input_size) {
        LOG_ERR("Input index %zu out of bounds for input size %zu", index, input_size);
        return;
    }
    input_buffer[index] = value;
    input_state[index] = true;

    for (size_t i = 0; i < input_size; ++i) {
        if (!input_state[i]) {
            return;
        }
    }

    sensor_value_t output;
    int ret = this->process(output);

    if (ret < 0) {
        LOG_ERR("Processing failed with error code %d", ret);
        return;
    } else if (ret > 0) {
        LOG_DBG("Processing returned %d, skipping ...", ret);
        return;
    }

    for (size_t i = 0; i < child_count; ++i) {
        if (children[i].stage) {
            children[i].stage->input(output, children[i].input_index);
        }
    }
}
