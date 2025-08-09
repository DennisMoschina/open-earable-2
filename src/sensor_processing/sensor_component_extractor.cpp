#include "sensor_component_extractor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_component_extractor, LOG_LEVEL_DBG);

SensorComponentExtractor::SensorComponentExtractor(size_t group_index, size_t component_index,
                                                     sensor_processing_stage_t *children, size_t child_count)
    : SensorProcessingStage(1, children, child_count), group_index(group_index), component_index(component_index) {
}

sensor_value_t SensorComponentExtractor::process() {
    sensor_value_t input_value = input_buffer[0];

    if (group_index >= input_value.group_count) {
        LOG_ERR("Group index %zu out of bounds for input size %zu", group_index, input_value.group_count);
        return sensor_value_t{0}; // Return an empty sensor_value_t
    }

    const sensor_value_group_t &group = input_value.groups[group_index];
    if (component_index >= group.component_count) {
        LOG_ERR("Component index %zu out of bounds for group with %zu components", component_index, group.component_count);
        return sensor_value_t{0}; // Return an empty sensor_value_t
    }

    sensor_value_component_t component = group.components[component_index];

    sensor_value_t output;
    output.group_count = 1;
    output.groups = new sensor_value_group_t[1];
    output.groups[0].name = group.name;
    output.groups[0].component_count = 1;
    output.groups[0].components = new sensor_value_component_t[1];
    output.groups[0].components[0] = component;
    output.name = "Extracted Component";
    output.timestamp = input_value.timestamp;

    return output;
}