#include "biquad_filter_stage.h"

BiQuadFilterStage::BiQuadFilterStage(enum ParseType input_type, BiQuadFilter &filter) : SensorProcessingStage(1) {
    this->filter = filter;
    this->parse_type = input_type;
}

BiQuadFilterStage::BiQuadFilterStage(enum ParseType input_type, size_t stages, float coefficients[][5]) : SensorProcessingStage(1) {
    this->parse_type = input_type;
    this->filter = BiQuadFilter(stages);
    this->filter.set_coefficients(coefficients, stages);
}

BiQuadFilterStage::~BiQuadFilterStage() {
    //TODO: implement
}

int BiQuadFilterStage::process(const struct sensor_data *const input[],
                               struct sensor_data *output) {
    // Copy metadata (id, timestamp, etc.)
    *output = *input[0];
    output->size = sizeof(float);   // we always output float
    float x = 0.0f;

    switch (parse_type) {
        case PARSE_TYPE_UINT8: {
            uint8_t v = *reinterpret_cast<const uint8_t*>(input[0]->data);
            x = static_cast<float>(v);
            break;
        }
        case PARSE_TYPE_INT8: {
            int8_t v = *reinterpret_cast<const int8_t*>(input[0]->data);
            x = static_cast<float>(v);
            break;
        }
        case PARSE_TYPE_UINT16: {
            uint16_t v = *reinterpret_cast<const uint16_t*>(input[0]->data);
            x = static_cast<float>(v);
            break;
        }
        case PARSE_TYPE_INT16: {
            int16_t v = *reinterpret_cast<const int16_t*>(input[0]->data);
            x = static_cast<float>(v);
            break;
        }
        case PARSE_TYPE_UINT32: {
            uint32_t v = *reinterpret_cast<const uint32_t*>(input[0]->data);
            x = static_cast<float>(v);
            break;
        }
        case PARSE_TYPE_INT32: {
            int32_t v = *reinterpret_cast<const int32_t*>(input[0]->data);
            x = static_cast<float>(v);
            break;
        }
        case PARSE_TYPE_DOUBLE: {
            double v = *reinterpret_cast<const double*>(input[0]->data);
            x = static_cast<float>(v);
            break;
        }
        default:
            return -EINVAL; // unknown type
    }

    // Filter expects a float array
    float y = x;
    filter.apply(&y, 1);

    // Store result back as float (4 bytes)
    *reinterpret_cast<float*>(output->data) = y;

    return 0;
}
