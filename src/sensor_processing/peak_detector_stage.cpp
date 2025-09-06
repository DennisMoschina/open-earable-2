#include "peak_detector_stage.h"
#include <cstring>


static inline float decode_as_float(ParseType t, const uint8_t* p) {
    switch (t) {
        case PARSE_TYPE_UINT8:  { uint8_t  v; std::memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_INT8:   { int8_t   v; std::memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_UINT16: { uint16_t v; std::memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_INT16:  { int16_t  v; std::memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_UINT32: { uint32_t v; std::memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_INT32:  { int32_t  v; std::memcpy(&v, p, sizeof(v)); return (float)v; }
        case PARSE_TYPE_FLOAT:  { float    v; std::memcpy(&v, p, sizeof(v)); return v; }
        case PARSE_TYPE_DOUBLE: { double   v; std::memcpy(&v, p, sizeof(v)); return (float)v; }
        default: return 0.0f;
    }
}

static inline int8_t peak_sign(float last_dx, float curr_dx) {
    if (last_dx < 0 && curr_dx > 0) return 1;   // positive peak
    if (last_dx > 0 && curr_dx < 0) return -1;  // negative peak
    return 0;                                  // no peak
}

PeakDetectorStage::PeakDetectorStage(enum ParseType parse_type)
    : SensorProcessingStage(1), parse_type(parse_type), last_data(nullptr), last_dx(0) {
}

PeakDetectorStage::~PeakDetectorStage() {
    delete last_data;
}

int PeakDetectorStage::process(const struct sensor_data *const input[], struct sensor_data *output) {
    if (!input) return -EINVAL;
    const struct sensor_data *in = input[0];

    if (!this->last_data) {
        this->last_data = new sensor_data(*in);
        return 1;
    }

    float curr_f = decode_sample(in);
    float last_f = decode_sample(this->last_data);
    float dx = curr_f - last_f;

    delete this->last_data;
    this->last_data = new sensor_data(*in);

    if (isnan(this->last_dx)) {
        this->last_dx = dx;
        return 1;
    }

    int8_t sign = peak_sign(this->last_dx, dx);
    if (sign == 0) {
        this->last_dx = dx;
        return 1;
    }

    this->last_dx = dx;
    
    // create a copy of in for output
    *output = *in;
    output->size = in->size + parseTypeSizes[PARSE_TYPE_INT8];
    int8_t peak_sign = -sign;
    std::memcpy(output->data, in->data, in->size);
    std::memcpy(output->data + in->size, &peak_sign, sizeof(peak_sign));

    return 0;
}

float PeakDetectorStage::decode_sample(const struct sensor_data* sd) const {
    if (!sd) return 0.0f;

    const uint8_t* p = sd->data;
    return decode_as_float(this->parse_type, p);
}