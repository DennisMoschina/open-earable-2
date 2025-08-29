#include "pipeline_python_sink.h"

#include <stdlib.h>
// sensor_bridge.c
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objlist.h"
#include "py/binary.h"

#include "ParseType.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openearable_pipeline_python_sink, LOG_LEVEL_DBG);

// Cached class refs (loaded once)
static mp_obj_t cls_SensorValue = MP_OBJ_NULL;
static mp_obj_t cls_SensorValueGroup = MP_OBJ_NULL;
static mp_obj_t cls_SensorValueComponent = MP_OBJ_NULL;

static void ensure_classes_loaded(void) {
    if (cls_SensorValue != MP_OBJ_NULL) return;

    LOG_DBG("Importing sensor classes");

    // import sensor_values
    // mp_obj_t mod = mp_import_name(MP_QSTR_sensor, mp_const_none, mp_const_none);
    // // load classes
    // cls_SensorValue         = mp_load_attr(mod, MP_QSTR_SensorValue);
    // cls_SensorValueGroup    = mp_load_attr(mod, MP_QSTR_SensorValueGroup);
    // cls_SensorValueComponent= mp_load_attr(mod, MP_QSTR_SensorValueComponent);
}

// Create SensorValueComponent(name: str, value: obj, unit: str)
static mp_obj_t make_component(const char *name, mp_obj_t value_obj, const char *unit) {
    mp_obj_t args[3] = {
        mp_obj_new_str(name, strlen(name)),
        value_obj,  // already an mp_obj_t (int/float/etc.)
        mp_obj_new_str(unit, strlen(unit))
    };
    return mp_call_function_n_kw(cls_SensorValueComponent, 3, 0, args);
}

// Create SensorValueGroup(name: str, components: list[SensorValueComponent])
static mp_obj_t make_group(const char *name, mp_obj_t comp_list) {
    mp_obj_t args[2] = {
        mp_obj_new_str(name, strlen(name)),
        comp_list
    };
    return mp_call_function_n_kw(cls_SensorValueGroup, 2, 0, args);
}

// Create SensorValue(name: str, timestamp: int, groups: list[SensorValueGroup])
static mp_obj_t make_value(const char *name, uint64_t ts, mp_obj_t group_list) {
    // Use int (small) or int_from_ull for 64-bit timestamps
    mp_obj_t ts_obj = mp_obj_new_int_from_ull(ts);
    mp_obj_t args[3] = {
        mp_obj_new_str(name, strlen(name)),
        ts_obj,
        group_list
    };
    return mp_call_function_n_kw(cls_SensorValue, 3, 0, args);
}

PipelinePythonSink::PipelinePythonSink(const struct SensorScheme &data_scheme, mp_obj_t py_callback)
    : SensorProcessingStage(1), data_scheme(data_scheme), py_callback(py_callback) {}

PipelinePythonSink::~PipelinePythonSink() {
    free((void*)data_scheme.name);
    for (size_t gi = 0; gi < data_scheme.groupCount; ++gi) {
        auto &grp = data_scheme.groups[gi];
        free((void*)grp.name);
        for (size_t ci = 0; ci < grp.componentCount; ++ci) {
            free((void*)grp.components[ci].name);
            free((void*)grp.components[ci].unit);
        }
        delete[] grp.components;
    }
    delete[] data_scheme.groups;
}

int PipelinePythonSink::process(const struct sensor_data *const input[], struct sensor_data *output) {
    mp_obj_t parsed_data = this->parse_data(input[0]);
    // mp_sched_schedule(py_callback, parsed_data);
    return 0;
}

mp_obj_t PipelinePythonSink::parse_data(const struct sensor_data *data) {
    ensure_classes_loaded();  // lazy import & cache Python classes

    LOG_DBG("Creating sensor value objects");

    // // 1. Make list of groups
    // mp_obj_list_t *group_list = (mp_obj_list_t*) MP_OBJ_TO_PTR(mp_obj_new_list(0, NULL));

    // size_t offset = 0;
    // for (size_t i = 0; i < this->data_scheme.groupCount; i++) {
    //     const struct SensorComponentGroup &group = this->data_scheme.groups[i];
    //     // 2. Make list of components for this group
    //     mp_obj_list_t *comp_list = (mp_obj_list_t*)MP_OBJ_TO_PTR(mp_obj_new_list(0, NULL));

    //     for (size_t j = 0; j < group.componentCount; j++) {
    //         const struct SensorComponent &comp = group.components[j];
    //         mp_obj_t value_obj = mp_const_none;

    //         // decode raw value from bytes at current offset
    //         switch (comp.parseType) {
    //             case PARSE_TYPE_UINT8: {
    //                 uint8_t v = *(uint8_t*)&data->data[offset];
    //                 value_obj = mp_obj_new_int(v);
    //                 offset += sizeof(uint8_t);
    //                 break;
    //             }
    //             case PARSE_TYPE_INT8: {
    //                 int8_t v = *(int8_t*)&data->data[offset];
    //                 value_obj = mp_obj_new_int(v);
    //                 offset += sizeof(int8_t);
    //                 break;
    //             }
    //             case PARSE_TYPE_UINT16: {
    //                 uint16_t v = *(uint16_t*)&data->data[offset];
    //                 value_obj = mp_obj_new_int(v);
    //                 offset += sizeof(uint16_t);
    //                 break;
    //             }
    //             case PARSE_TYPE_INT16: {
    //                 int16_t v = *(int16_t*)&data->data[offset];
    //                 value_obj = mp_obj_new_int(v);
    //                 offset += sizeof(int16_t);
    //                 break;
    //             }
    //             case PARSE_TYPE_UINT32: {
    //                 uint32_t v = *(uint32_t*)&data->data[offset];
    //                 value_obj = mp_obj_new_int_from_uint(v);
    //                 offset += sizeof(uint32_t);
    //                 break;
    //             }
    //             case PARSE_TYPE_INT32: {
    //                 int32_t v = *(int32_t*)&data->data[offset];
    //                 value_obj = mp_obj_new_int(v);
    //                 offset += sizeof(int32_t);
    //                 break;
    //             }
    //             case PARSE_TYPE_FLOAT: {
    //                 float v = *(float*)&data->data[offset];
    //                 value_obj = mp_obj_new_float(v);
    //                 offset += sizeof(float);
    //                 break;
    //             }
    //             case PARSE_TYPE_DOUBLE: {
    //                 double v = *(double*)&data->data[offset];
    //                 value_obj = mp_obj_new_float(v);  // downcast to float in Python
    //                 offset += sizeof(double);
    //                 break;
    //             }
    //             default:
    //                 value_obj = mp_const_none;
    //                 break;
    //         }

    //         // 3. Make SensorValueComponent(name, value, unit)
    //         mp_obj_t comp_obj = make_component(comp.name, value_obj, comp.unit);
    //         mp_obj_list_append((mp_obj_list_t*) MP_OBJ_FROM_PTR(comp_list), comp_obj);
    //     }

    //     // 4. Make SensorValueGroup(name, components)
    //     mp_obj_t group_obj = make_group(group.name, MP_OBJ_FROM_PTR(comp_list));
    //     mp_obj_list_append((mp_obj_list_t*) MP_OBJ_FROM_PTR(group_list), group_obj);
    // }

    // // 5. Make top-level SensorValue(name, timestamp, groups)
    // mp_obj_t sv_obj = make_value(data_scheme.name, data->time, MP_OBJ_FROM_PTR(group_list));

    // return sv_obj;
    return mp_const_none;
}
