#ifndef _ZERO_CROSSING_DETECTOR_STAGE_H
#define _ZERO_CROSSING_DETECTOR_STAGE_H

#include "sensor_processing_stage.h"
#include "ParseType.h"

class ZeroCrossingDetectorStage : public SensorProcessingStage {
public:
    ZeroCrossingDetectorStage(enum ParseType parse_type);

    int process(const struct sensor_data *const inputs[], struct sensor_data *output) override;

private:
    struct sensor_data last_value;
    bool is_initialized;
    enum ParseType parse_type;
};

#endif // _ZERO_CROSSING_DETECTOR_STAGE_H