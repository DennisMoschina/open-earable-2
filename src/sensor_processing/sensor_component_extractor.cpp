#include "sensor_component_extractor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_component_extractor, LOG_LEVEL_DBG);

SensorComponentExtractor::SensorComponentExtractor(size_t offset, enum ParseType parse_type)
    : SensorProcessingStage(1), offset(offset), parse_type(parse_type) {
}

int SensorComponentExtractor::process(const struct sensor_data *const inputs[],
                                      struct sensor_data *output) {
    const struct sensor_data *input = inputs[0];

    // Copy metadata
    *output = *input;

    switch (this->parse_type) {
        case PARSE_TYPE_UINT8: {
            auto v = reinterpret_cast<const uint8_t*>(input->data)[this->offset];
            *reinterpret_cast<uint8_t*>(output->data) = v;
            output->size = sizeof(uint8_t);
            break;
        }
        case PARSE_TYPE_INT8: {
            auto v = reinterpret_cast<const int8_t*>(input->data)[this->offset];
            *reinterpret_cast<int8_t*>(output->data) = v;
            output->size = sizeof(int8_t);
            break;
        }
        case PARSE_TYPE_UINT16: {
            auto v = reinterpret_cast<const uint16_t*>(input->data)[this->offset];
            *reinterpret_cast<uint16_t*>(output->data) = v;
            output->size = sizeof(uint16_t);
            break;
        }
        case PARSE_TYPE_INT16: {
            auto v = reinterpret_cast<const int16_t*>(input->data)[this->offset];
            *reinterpret_cast<int16_t*>(output->data) = v;
            output->size = sizeof(int16_t);
            break;
        }
        case PARSE_TYPE_UINT32: {
            auto v = reinterpret_cast<const uint32_t*>(input->data)[this->offset];
            *reinterpret_cast<uint32_t*>(output->data) = v;
            output->size = sizeof(uint32_t);
            break;
        }
        case PARSE_TYPE_INT32: {
            auto v = reinterpret_cast<const int32_t*>(input->data)[this->offset];
            *reinterpret_cast<int32_t*>(output->data) = v;
            output->size = sizeof(int32_t);
            break;
        }
        case PARSE_TYPE_FLOAT: {
            auto v = reinterpret_cast<const float*>(input->data)[this->offset];
            *reinterpret_cast<float*>(output->data) = v;
            output->size = sizeof(float);
            break;
        }
        case PARSE_TYPE_DOUBLE: {
            auto v = reinterpret_cast<const double*>(input->data)[this->offset];
            *reinterpret_cast<double*>(output->data) = v;
            output->size = sizeof(double);
            break;
        }
        default:
            LOG_ERR("Unsupported parse type");
            return -1;
    }

    return 0;
}
