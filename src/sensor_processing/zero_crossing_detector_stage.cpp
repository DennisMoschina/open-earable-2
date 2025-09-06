#include "zero_crossing_detector_stage.h"
#include <cstring>            // memcpy
#include "zephyr/logging/log.h"
LOG_MODULE_REGISTER(zero_crossing_detector_stage, LOG_LEVEL_WRN);

static inline int8_t sign_crossing(double last, double cur) {
    if (last < 0 && cur > 0)  return  1; // rising through zero
    if (last > 0 && cur < 0)  return -1; // falling through zero
    return 0;
}

ZeroCrossingDetectorStage::ZeroCrossingDetectorStage(ParseType pt)
    : SensorProcessingStage(1),
      parse_type(pt),
      is_initialized(false),
      last_scalar(0.0) {}

int ZeroCrossingDetectorStage::process(const struct sensor_data *const inputs[],
                                       struct sensor_data *output) {
    const struct sensor_data* in = inputs[0];

    // Decode current sample into a scalar safely (handles unaligned buffers)
    double cur = 0.0;

    switch (parse_type) {
    case PARSE_TYPE_UINT8:
    case PARSE_TYPE_UINT16:
    case PARSE_TYPE_UINT32:
        LOG_ERR("Zero crossing not defined for unsigned integer types");
        return -EINVAL;

    case PARSE_TYPE_INT8: {
        int8_t v;  std::memcpy(&v, in->data, sizeof(v));
        cur = static_cast<double>(v);
        break;
    }
    case PARSE_TYPE_INT16: {
        int16_t v; std::memcpy(&v, in->data, sizeof(v));
        cur = static_cast<double>(v);
        break;
    }
    case PARSE_TYPE_INT32: {
        int32_t v; std::memcpy(&v, in->data, sizeof(v));
        cur = static_cast<double>(v);
        break;
    }
    case PARSE_TYPE_FLOAT: {
        float v;   std::memcpy(&v, in->data, sizeof(v));
        cur = static_cast<double>(v);
        break;
    }
    case PARSE_TYPE_DOUBLE: {
        double v;  std::memcpy(&v, in->data, sizeof(v));
        cur = v;
        break;
    }
    default:
        LOG_ERR("Unsupported parse type");
        return -EINVAL;
    }

    LOG_DBG("Time: %lld Value: %f", in->time, cur);
    
    if (!is_initialized) {
        last_scalar = cur;
        is_initialized = true;
        return 1; // no event on first sample
    }
    
    const int8_t crossing = sign_crossing(last_scalar, cur);
    last_scalar = cur; // update state immediately
    
    if (crossing == 0) {
        return 1; // no event this tick
    }
    
    // Emit event: reuse input metadata, payload is int8 {-1, +1}
    *output = *in;
    output->size = sizeof(int8_t);
    std::memcpy(output->data, &crossing, sizeof(crossing));

    return 0;
}