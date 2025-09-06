#ifndef _PEAK_DETECTOR_STAGE_H
#define _PEAK_DETECTOR_STAGE_H

#include "sensor_processing_stage.h"

#include "ParseType.h"
#include <math.h>  // isnan

class PeakDetectorStage : public SensorProcessingStage {
public:
    PeakDetectorStage(enum ParseType parse_type);
    ~PeakDetectorStage();

    int process(const struct sensor_data *const input[], struct sensor_data *output) override;

private:
    enum ParseType parse_type;
    const struct sensor_data *last_data;
    float last_dx = NAN;

    float decode_sample(const struct sensor_data* sd) const;
};

#endif // _PEAK_DETECTOR_STAGE_H