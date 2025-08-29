#include "modpipeline_processing.h"

#include <array>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objlist.h"
#include "py/binary.h"

#include "processing_pipeline.h"
#include "sensor_processing_consumer.h"
#include "sensor_source_stage.h"
#include "pipeline_python_sink.h"
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

mp_obj_t openearable_processing_pipeline_add_source(mp_obj_t pipeline_name, mp_obj_t stage_name, mp_obj_t stage) {
    const char *name = mp_obj_str_get_str(pipeline_name);
    ProcessingPipeline *pipeline = get_processing_pipeline(name);
    SensorProcessingStage *sensor_stage = nullptr;
    int ret = build_processing_stage(stage, sensor_stage);
    if (ret != 0) {
        return mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to build processing stage");
    }
    std::unique_ptr<SensorProcessingStage> stage_ptr(sensor_stage);
    const char *stage_str = mp_obj_str_get_str(stage_name);
    pipeline->add_source(stage_str, std::move(stage_ptr));
    return mp_const_none;
}

mp_obj_t openearable_connect_stages(size_t n_args,
                                    const mp_obj_t *pos_args,
                                    mp_map_t *kw_args) {
    // Define accepted args (names must match what you use in Python)
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_pipeline, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_src,      MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_dst,      MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_dst_port, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args,
                     MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    const char *pipeline_name = mp_obj_str_get_str(args[0].u_obj);
    const char *src_name      = mp_obj_str_get_str(args[1].u_obj);
    const char *dst_name      = mp_obj_str_get_str(args[2].u_obj);
    size_t      dst_port      = (size_t)args[3].u_int;

    ProcessingPipeline *pipeline = get_processing_pipeline(pipeline_name);
    if (!pipeline) {
        mp_raise_ValueError(MP_ERROR_TEXT("Pipeline not found"));
    }

    pipeline->connect(src_name, dst_name, dst_port);

    return mp_const_none;
}

int build_processing_stage(mp_obj_t stage, SensorProcessingStage *sensor_stage) {
    LOG_DBG("Building processing stage");
    if (!mp_obj_is_type(stage, &mp_type_tuple)) {
        LOG_ERR("Stage description must be a tuple");
        return -EINVAL;
    }

    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(stage, &len, &items);

    // use python print to print the tuple content
    mp_obj_print(stage, PRINT_STR);

    if (len < 2) {
        LOG_ERR("Stage tuple has to contain at least 2 items");
        return -EINVAL;
    }

    int kind = mp_obj_get_int(items[0]);
    LOG_DBG("Processing stage kind: %d", kind);

    switch (kind) {
    case STAGE_SENSOR: {
        if (len < 2) return -EINVAL;
        int sensor_id = mp_obj_get_int(items[2]);
        sensor_stage = new SensorSourceStage(sensor_id);
        return 0;
    }
    case STAGE_PYTHON_SINK: {
        if (len < 4) return -EINVAL;

        mp_obj_t py_callback = items[2];
        mp_obj_t scheme_obj  = items[3];

        // ---- Extract name ----
        mp_obj_t name_obj = mp_load_attr(scheme_obj, MP_QSTR_name);
        const char *name  = mp_obj_str_get_str(name_obj);

        // ---- Extract id ----
        mp_obj_t id_obj = mp_load_attr(scheme_obj, MP_QSTR_id);
        uint8_t id      = (uint8_t)mp_obj_get_int(id_obj);

        // ---- Extract groups ----
        mp_obj_t groups_obj = mp_load_attr(scheme_obj, MP_QSTR_groups);
        size_t group_len;
        mp_obj_t *group_items;
        mp_obj_get_array(groups_obj, &group_len, &group_items);

        // Allocate groups
        struct SensorComponentGroup *groups = (SensorComponentGroup*)k_malloc(sizeof(SensorComponentGroup) * group_len);

        for (size_t gi = 0; gi < group_len; ++gi) {
            mp_obj_t group_obj = group_items[gi];

            // Group name
            mp_obj_t gname_obj = mp_load_attr(group_obj, MP_QSTR_name);
            const char *gname  = mp_obj_str_get_str(gname_obj);

            // Components
            mp_obj_t comps_obj = mp_load_attr(group_obj, MP_QSTR_components);
            size_t comp_len;
            mp_obj_t *comp_items;
            mp_obj_get_array(comps_obj, &comp_len, &comp_items);

            struct SensorComponent *components = (SensorComponent*)k_malloc(sizeof(SensorComponent) * comp_len);

            for (size_t ci = 0; ci < comp_len; ++ci) {
                mp_obj_t comp_obj = comp_items[ci];

                mp_obj_t cname_obj = mp_load_attr(comp_obj, MP_QSTR_name);
                mp_obj_t unit_obj  = mp_load_attr(comp_obj, MP_QSTR_unit);
                mp_obj_t pt_obj    = mp_load_attr(comp_obj, MP_QSTR_parse_type);

                const char *cname  = mp_obj_str_get_str(cname_obj);
                const char *unit   = mp_obj_str_get_str(unit_obj);
                enum ParseType pt = static_cast<ParseType>(mp_obj_get_int(pt_obj));

                components[ci] = {
                    .name = cname,
                    .unit = unit,
                    .parseType = pt
                };
            }

            groups[gi] = {
                .name = gname,
                .componentCount = comp_len,
                .components = components
            };
        }

        // ---- Build C SensorScheme ----
        struct SensorScheme scheme = {
            .name = name,
            .id = id,
            .groupCount = (uint8_t)group_len,
            .groups = groups,
            .configOptions = {} // ignore options for now
        };

        // ---- Pass to stage ----
        sensor_stage = new PipelinePythonSink(scheme, py_callback);
        return 0;
    }
    case STAGE_BIQUAD: {
        if (len < 5) return -EINVAL;
        enum ParseType parse_type = static_cast<ParseType>(mp_obj_get_int(items[2]));
        int stages = mp_obj_get_int(items[3]);

        // items[4] is a nested list of lists of coeffs
        size_t outer_len;
        mp_obj_t *outer_items;
        mp_obj_get_array(items[4], &outer_len, &outer_items);

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
        enum ParseType parse_type = static_cast<ParseType>(mp_obj_get_int(items[2]));
        sensor_stage = new ZeroCrossingDetectorStage(parse_type);
        return 0;
    }
    case STAGE_COMP_EXTRACTOR: {
        if (len < 4) return -EINVAL;
        enum ParseType parse_type = static_cast<ParseType>(mp_obj_get_int(items[2]));
        int comp_offset = mp_obj_get_int(items[3]);
        sensor_stage = new SensorComponentExtractor(comp_offset, parse_type);
        return 0;
    }
    default:
        LOG_ERR("Unknown stage kind: %d", kind);
        return -EINVAL;
    }
}