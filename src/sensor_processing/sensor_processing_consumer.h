#ifndef _SENSOR_PROCESSING_CONSUMER_H
#define _SENSOR_PROCESSING_CONSUMER_H

#include "sensor_processing_stage.h"

void set_processing_pipeline(SensorProcessingStage *stage, uint8_t sensor_id);
void remove_processing_pipeline(uint8_t sensor_id);

#endif