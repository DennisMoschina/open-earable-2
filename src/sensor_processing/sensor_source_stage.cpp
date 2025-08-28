#include "sensor_source_stage.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_source_stage, LOG_LEVEL_DBG);

SensorSourceStage::SensorSourceStage(uint8_t sensor_id) : SensorProcessingStage(1), sensor_id(sensor_id) {
    // Constructor implementation
}

int SensorSourceStage::process(const struct sensor_data *const input[], struct sensor_data *output) {
    if (input[0]->id != sensor_id) {
        LOG_ERR("Invalid sensor ID: %d (expected: %d)", input[0]->id, sensor_id);
        return -EINVAL; // Invalid sensor ID
    }
    *output = *input[0];
    return 0;
}

uint8_t SensorSourceStage::get_sensor_id() const {
    return sensor_id;
}
