#include "zero_crossing_detector_stage.h"
#include "ParseType.h"

#include "zephyr/logging/log.h"
LOG_MODULE_REGISTER(zero_crossing_detector_stage, LOG_LEVEL_DBG);

template<typename T>
int8_t is_crossing(T last, T current) {
    if (last < 0 && current > 0) {
        return 1; // Positive crossing
    } else if (last > 0 && current < 0) {
        return -1; // Negative crossing
    }
    return 0; // No crossing
}

ZeroCrossingDetectorStage::ZeroCrossingDetectorStage(enum ParseType parse_type)
    : SensorProcessingStage(1), last_value(), is_initialized(false), parse_type(parse_type) { }

int ZeroCrossingDetectorStage::process(const struct sensor_data *const inputs[], struct sensor_data *output) {
    if (!is_initialized) {
        last_value = *inputs[0];
        is_initialized = true;
        return 1;
    }

    int8_t crossing;
    switch (this->parse_type) {
        case PARSE_TYPE_UINT8:
        case PARSE_TYPE_UINT16:
        case PARSE_TYPE_UINT32:
            LOG_ERR("Can't detect zero crossing for unsigned integer types");
            return -1;
        case PARSE_TYPE_INT8:
            crossing = is_crossing<int8_t>(*(int8_t*) this->last_value.data, *(int8_t*) inputs[0]->data);
            break;
        case PARSE_TYPE_INT16:
            crossing = is_crossing<int16_t>(*(int16_t*) this->last_value.data, *(int16_t*) inputs[0]->data);
            break;
        case PARSE_TYPE_INT32:
            crossing = is_crossing<int32_t>(*(int32_t*) this->last_value.data, *(int32_t*) inputs[0]->data);
            break;
        case PARSE_TYPE_FLOAT:
            crossing = is_crossing<float>(*(float*) this->last_value.data, *(float*) inputs[0]->data);
            break;
        case PARSE_TYPE_DOUBLE:
            crossing = is_crossing<double>(*(double*) this->last_value.data, *(double*) inputs[0]->data);
            break;
        default:
            LOG_ERR("Unsupported parse type");
            return -1;
    }

    last_value = *inputs[0];
    if (crossing == 0) {
        return 1;
    }
    *output = *inputs[0];
    //TODO: actually set value
    // output->data = crossing;
    return 0;
}