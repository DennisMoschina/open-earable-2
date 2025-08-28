#include "modpipeline_processing.h"

#include "processing_pipeline.h"
#include "sensor_processing_consumer.h"
#include <array>

#include "sensor_source_stage.h"
// #include "python_sink_stage.h"
#include "zero_crossing_detector_stage.h"
#include "biquad_filter_stage.h"
#include "sensor_component_extractor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openearable_modpipeline_processing, LOG_LEVEL_DBG);

int build_processing_stage(mp_obj_t stage, SensorProcessingStage *sensor_stage);

enum ProcessingStageKind {
    STAGE_SENSOR = 0,
    STAGE_PYTHON_SINK = 1,
    STAGE_BIQUAD = 2,
    STAGE_PEAK = 3,
    STAGE_ZC = 4,
    STAGE_COMP_EXTRACTOR = 5
};

mp_obj_t openearable_create_processing_pipeline(mp_obj_t name) {
    const char *pipeline_name = mp_obj_str_get_str(name);
    auto p = std::make_unique<ProcessingPipeline>();
    set_processing_pipeline(pipeline_name, std::move(p));
    return mp_const_none;
}

mp_obj_t openearable_remove_processing_pipeline(mp_obj_t name) {
    const char *pipeline_name = mp_obj_str_get_str(name);
    remove_processing_pipeline(pipeline_name);
    return mp_const_none;
}

mp_obj_t openearable_processing_pipeline_add_stage(mp_obj_t pipeline_name, mp_obj_t stage_name, mp_obj_t stage) {
    const char *name = mp_obj_str_get_str(pipeline_name);
    ProcessingPipeline *pipeline = get_processing_pipeline(name);
    SensorProcessingStage *sensor_stage = nullptr;
    int ret = build_processing_stage(stage, sensor_stage);
    if (ret != 0) {
        return mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to build processing stage");
    }
    std::unique_ptr<SensorProcessingStage> stage_ptr(sensor_stage);
    const char *stage_str = mp_obj_str_get_str(stage_name);
    pipeline->add_stage(stage_str, std::move(stage_ptr));
    return mp_const_none;
}

mp_obj_t openearable_connect_stages(size_t n_args, const mp_obj_t *args) {
    mp_arg_check_num(n_args, 4, 4, 4, false);

    const char *pipeline_name = mp_obj_str_get_str(args[0]);
    const char *src_name      = mp_obj_str_get_str(args[1]);
    const char *dst_name      = mp_obj_str_get_str(args[2]);
    size_t dst_port           = mp_obj_get_int(args[3]);

    ProcessingPipeline *pipeline = get_processing_pipeline(pipeline_name);
    if (!pipeline) {
        mp_raise_ValueError(MP_ERROR_TEXT("Pipeline not found"));
    }

    pipeline->connect(src_name, dst_name, dst_port);

    return mp_const_none;
}

int build_processing_stage(mp_obj_t stage, SensorProcessingStage *sensor_stage) {
    if (!mp_obj_is_type(stage, &mp_type_tuple)) {
        LOG_ERR("Stage description must be a tuple");
        return -EINVAL;
    }

    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(stage, &len, &items);

    if (len < 1) {
        LOG_ERR("Stage tuple is empty");
        return -EINVAL;
    }

    int kind = mp_obj_get_int(items[0]);

    switch (kind) {
    case STAGE_SENSOR: {
        if (len < 2) return -EINVAL;
        int sensor_id = mp_obj_get_int(items[1]);
        sensor_stage = new SensorSourceStage(sensor_id);
        return 0;
    }
    // case STAGE_PYTHON_SINK:
    //     sensor_stage = new PythonSinkStage();
    //     return 0;
    case STAGE_BIQUAD: {
        if (len < 3) return -EINVAL;
        enum ParseType parse_type = static_cast<ParseType>(mp_obj_get_int(items[1]));
        int stages = mp_obj_get_int(items[2]);

        // items[2] is a nested list of lists of coeffs
        size_t outer_len;
        mp_obj_t *outer_items;
        mp_obj_get_array(items[2], &outer_len, &outer_items);

        std::vector<std::array<float,5>> coeffs;
        coeffs.reserve(outer_len);

        for (size_t i = 0; i < outer_len; ++i) {
            size_t inner_len;
            mp_obj_t *inner_items;
            mp_obj_get_array(outer_items[i], &inner_len, &inner_items);
            if (inner_len != 5) return -EINVAL;
            std::array<float,5> c;
            for (size_t j=0;j<5;j++) {
                c[j] = mp_obj_get_float(inner_items[j]);
            }
            coeffs.push_back(c);
        }

        float c_coeffs[10][5];  // Assuming max 10 stages
        for (size_t i = 0; i < coeffs.size(); i++) {
            for (size_t j = 0; j < 5; j++) {
            c_coeffs[i][j] = coeffs[i][j];
            }
        }
        sensor_stage = new BiQuadFilterStage(parse_type, stages, c_coeffs);
        return 0;
    }
    // case STAGE_PEAK:
    //     sensor_stage = new PeakDetectorStage();
    //     return 0;
    case STAGE_ZC: {
        enum ParseType parse_type = static_cast<ParseType>(mp_obj_get_int(items[1]));
        sensor_stage = new ZeroCrossingDetectorStage(parse_type);
        return 0;
    }
    case STAGE_COMP_EXTRACTOR: {
        if (len < 2) return -EINVAL;
        enum ParseType parse_type = static_cast<ParseType>(mp_obj_get_int(items[1]));
        int comp_offset = mp_obj_get_int(items[2]);
        sensor_stage = new SensorComponentExtractor(comp_offset, parse_type);
        return 0;
    }
    default:
        LOG_ERR("Unknown stage kind: %d", kind);
        return -EINVAL;
    }
}