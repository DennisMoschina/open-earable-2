#include "modpipeline_processing.h"

#include <array>
#include <limits>

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
#include "peak_detector_stage.h"
#include "biquad_filter_stage.h"
#include "sensor_component_extractor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openearable_modpipeline_processing, LOG_LEVEL_DBG);

int build_processing_stage(mp_obj_t stage, SensorProcessingStage *&out_stage);
int get_stage_info(mp_obj_t stage_obj,
                   int *out_kind,
                   const char **out_name,
                   mp_obj_t **out_items,
                   size_t *out_item_count);
int build_sensor_source_stage(mp_obj_t sensor_id_obj,
                              SensorSourceStage *&sensor_stage);

static char *dup_str(const char *s) {
    if (!s) return nullptr;
    size_t len = strlen(s) + 1;
    char *copy = new char[len];
    memcpy(copy, s, len);
    return copy;
}


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
        LOG_ERR("Failed to build processing stage %d", ret);
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

    int kind;
    const char *source_name;
    mp_obj_t *items;
    size_t item_count;
    int ret = get_stage_info(stage, &kind, &source_name, &items, &item_count);
    if (ret) {
        LOG_ERR("Failed to get stage info");
        return mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to get stage info");
    }

    if (kind != STAGE_SENSOR) {
        return mp_obj_new_exception_msg(&mp_type_ValueError, "Stage is not a sensor source");
    }

    SensorSourceStage *sensor_stage = nullptr;
    ret = build_sensor_source_stage(items[0], sensor_stage);
    if (ret != 0) {
        return mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to build processing stage");
    }
    std::unique_ptr<SensorSourceStage> stage_ptr(sensor_stage);
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

int build_sensor_source_stage(mp_obj_t sensor_id_obj,
                              SensorSourceStage *&sensor_stage) {
    sensor_stage = nullptr;  // clear out param upfront

    if (!mp_obj_is_int(sensor_id_obj)) {
        LOG_ERR("Sensor ID must be an integer");
        return -EINVAL;
    }

    mp_int_t v = mp_obj_get_int(sensor_id_obj);
    if (v < 0 || v > std::numeric_limits<uint8_t>::max()) {
        LOG_ERR("Sensor ID out of range: %ld", (long)v);
        return -EINVAL;
    }

    uint8_t sensor_id = static_cast<uint8_t>(v);
    LOG_DBG("Creating sensor source for ID %u", sensor_id);

    sensor_stage = new SensorSourceStage(sensor_id);
    return 0;
}

int build_python_sink(mp_obj_t py_callback,
                      mp_obj_t scheme_obj,
                      SensorProcessingStage *&out_stage) {
    if (!mp_obj_is_callable(py_callback)) {
        LOG_ERR("Callback is not callable");
        return -EINVAL;
    }

    // name
    const char *name = mp_obj_str_get_str(mp_load_attr(scheme_obj, MP_QSTR_name));
    // id
    uint8_t id = (uint8_t)mp_obj_get_int(mp_load_attr(scheme_obj, MP_QSTR_id));

    // groups
    mp_obj_t groups_obj = mp_load_attr(scheme_obj, MP_QSTR_groups);
    size_t group_len; mp_obj_t *group_items;
    mp_obj_get_array(groups_obj, &group_len, &group_items);

    // deep copy groups/components
    struct SensorComponentGroup *groups = new struct SensorComponentGroup[group_len];

    for (size_t gi = 0; gi < group_len; ++gi) {
        mp_obj_t gobj = group_items[gi];
        const char *gname = mp_obj_str_get_str(mp_load_attr(gobj, MP_QSTR_name));

        mp_obj_t comps_obj = mp_load_attr(gobj, MP_QSTR_components);
        size_t comp_len; mp_obj_t *comp_items;
        mp_obj_get_array(comps_obj, &comp_len, &comp_items);

        struct SensorComponent *components = new struct SensorComponent[comp_len];

        for (size_t ci = 0; ci < comp_len; ++ci) {
            mp_obj_t cobj = comp_items[ci];
            const char *cname = mp_obj_str_get_str(mp_load_attr(cobj, MP_QSTR_name));
            const char *unit  = mp_obj_str_get_str(mp_load_attr(cobj, MP_QSTR_unit));
            int pt            = mp_obj_get_int(mp_load_attr(cobj, MP_QSTR_parse_type));

            components[ci] = SensorComponent{
                .name = dup_str(cname),    // own it
                .unit = dup_str(unit),     // own it
                .parseType = (ParseType)pt
            };
        }

        groups[gi] = SensorComponentGroup{
            .name = dup_str(gname),       // own it
            .componentCount = comp_len,
            .components = components
        };
    }

    struct SensorScheme scheme = {
        .name = dup_str(name),            // own it
        .id = id,
        .groupCount = (uint8_t)group_len,
        .groups = groups,
        .configOptions = {}              // ignored
    };

    // Construct a stage that TAKES OWNERSHIP and frees in its destructor
    out_stage = new PipelinePythonSink(scheme, py_callback);
    return 0;
}

int build_biquad_filter_stage(mp_obj_t parse_type_obj,
                              mp_obj_t stage_count_obj,
                              mp_obj_t coeffs_obj,
                              SensorProcessingStage *&out_stage) {
    if (!MP_OBJ_IS_INT(parse_type_obj)) {
        LOG_ERR("Parse type must be an integer");
        return -EINVAL;
    }

    if (!MP_OBJ_IS_INT(stage_count_obj)) {
        LOG_ERR("Stage count must be an integer");
        return -EINVAL;
    }

    if (!MP_OBJ_IS_TYPE(coeffs_obj, &mp_type_list)) {
        LOG_ERR("Coefficients must be a list");
        return -EINVAL;
    }

    ParseType parse_type = (ParseType)mp_obj_get_int(parse_type_obj);
    int stages = mp_obj_get_int(stage_count_obj);
    if (stages <= 0) return -EINVAL;

    size_t outer_len; mp_obj_t *outer_items;
    mp_obj_get_array(coeffs_obj, &outer_len, &outer_items);
    if ((int)outer_len != stages) return -EINVAL;

    float coeffs[stages][5];
    for (size_t i = 0; i < outer_len; ++i) {
        size_t inner_len;
        mp_obj_t *inner_items;
        mp_obj_get_array(outer_items[i], &inner_len, &inner_items);
        if (inner_len != 5) return -EINVAL;
        for (size_t j = 0; j < 5; ++j) {
            coeffs[i][j] = mp_obj_get_float(inner_items[j]);
        }
    }

    // Let the stage COPY these coeffs internally (best design).
    out_stage = new BiQuadFilterStage(parse_type, stages, coeffs);
    return 0;
}

int get_stage_info(mp_obj_t stage_obj,
                   int *out_kind,
                   const char **out_name,
                   mp_obj_t **out_items,
                   size_t *out_item_count) {
    if (!mp_obj_is_type(stage_obj, &mp_type_tuple)) {
        LOG_ERR("Stage must be a tuple");
        return -EINVAL;
    }
    size_t len; mp_obj_t *arr;
    mp_obj_tuple_get(stage_obj, &len, &arr);
    if (len < 2) {
        LOG_ERR("Stage tuple needs at least (kind, name)");
        return -EINVAL;
    }

    mp_obj_print(stage_obj, PRINT_JSON);

    *out_kind = mp_obj_get_int(arr[0]);
    *out_name = mp_obj_str_get_str(arr[1]);

    // Return pointer to the WHOLE tuple array so caller can index arr[2..]
    *out_items = &arr[2];
    *out_item_count = len - 2;
    return 0;
}

int build_processing_stage(mp_obj_t stage, SensorProcessingStage *&out_stage) {
    LOG_DBG("Building processing stage");

    int kind;
    const char *name;
    mp_obj_t *arr; size_t len;
    int ret = get_stage_info(stage, &kind, &name, &arr, &len);
    if (ret) return ret;

    // tuple layout we expect: (kind, name, arg0, arg1, ...)
    switch (kind) {
    case STAGE_SENSOR: {
        if (len < 1) return -EINVAL;

        SensorSourceStage* src_stage = nullptr;
        ret = build_sensor_source_stage(arr[0], src_stage);
        if (ret) return ret;

        // Upcast to base and hand back
        out_stage = static_cast<SensorProcessingStage*>(src_stage);
        return 0;
    }
    case STAGE_PYTHON_SINK: {
        if (len < 2) return -EINVAL;
        mp_obj_t py_callback = arr[0];
        mp_obj_t scheme_obj  = arr[1];
        ret = build_python_sink(py_callback, scheme_obj, out_stage); // note argument order
        if (ret) return ret;
        return 0;
    }
    case STAGE_BIQUAD: {
        if (len < 3) return -EINVAL;
        ret = build_biquad_filter_stage(arr[0], arr[1], arr[2], out_stage);
        if (ret) return ret;
        return 0;
    }
    case STAGE_PEAK: {
        if (len < 1) return -EINVAL;
        enum ParseType parse_type = (ParseType)mp_obj_get_int(arr[0]);
        out_stage = new PeakDetectorStage(parse_type);
        return 0;
    }
    case STAGE_ZC: {
        if (len < 1) return -EINVAL;
        enum ParseType parse_type = (ParseType)mp_obj_get_int(arr[0]);
        out_stage = new ZeroCrossingDetectorStage(parse_type);
        return 0;
    }
    case STAGE_COMP_EXTRACTOR: {
        if (len < 2) return -EINVAL;
        enum ParseType parse_type = (ParseType)mp_obj_get_int(arr[0]);
        int comp_offset = mp_obj_get_int(arr[1]);
        out_stage = new SensorComponentExtractor(comp_offset, parse_type);
        return 0;
    }
    default:
        LOG_ERR("Unknown stage kind: %d", kind);
        return -EINVAL;
    }
}