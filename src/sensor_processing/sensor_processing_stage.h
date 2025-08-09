#ifndef _SENSOR_PROCESSING_STAGE_H
#define _SENSOR_PROCESSING_STAGE_H

#include "sensor_value.h"

class SensorProcessingStage;

typedef struct sensor_processing_stage {
    SensorProcessingStage* stage;
    size_t input_index;
} sensor_processing_stage_t;

class SensorProcessingStage {
public:
    SensorProcessingStage(size_t input_size, sensor_processing_stage_t *children, size_t child_count);
    ~SensorProcessingStage();

    virtual sensor_value_t process() = 0;

    void input(sensor_value_t value, size_t index);

    size_t get_input_size() const {
        return input_size;
    }

protected:
    sensor_value_t *input_buffer;
    size_t input_size;

private:
    sensor_processing_stage_t *children;
    size_t child_count;
    bool *input_state;
};

#endif // _SENSOR_PROCESSING_STAGE_H