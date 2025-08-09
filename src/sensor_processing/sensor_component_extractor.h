#ifndef _SENSOR_COMPONENT_EXTRACTOR_H
#define _SENSOR_COMPONENT_EXTRACTOR_H

#include "sensor_value.h"
#include "sensor_processing_stage.h"

class SensorComponentExtractor: public SensorProcessingStage {
public:
    SensorComponentExtractor(size_t group_index, size_t component_index, sensor_processing_stage_t *children, size_t child_count);

    sensor_value_t process() override;

private:
    size_t group_index;
    size_t component_index;
};
