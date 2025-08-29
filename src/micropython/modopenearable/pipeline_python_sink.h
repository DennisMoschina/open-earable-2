#ifndef PIPELINE_PYTHON_SINK_H
#define PIPELINE_PYTHON_SINK_H

#include "sensor_processing_stage.h"
#include "SensorScheme.h"
#include "py/obj.h"

class PipelinePythonSink : public SensorProcessingStage {
public:
    PipelinePythonSink(const struct SensorScheme &data_scheme, mp_obj_t py_callback);
    ~PipelinePythonSink() override;

    int process(const struct sensor_data *const input[], struct sensor_data *output) override;

private:
    const struct SensorScheme data_scheme;
    
    mp_obj_t py_callback; // Python callback to be called with processed data

    mp_obj_t sensor_value_prototype;

    mp_obj_t parse_data(const struct sensor_data *data);

};

#endif // PIPELINE_PYTHON_SINK_H