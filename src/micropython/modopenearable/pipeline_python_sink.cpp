#include "pipeline_python_sink.h"

// sensor_bridge.c
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objlist.h"
#include "py/binary.h"

#define MOD_QSTR         MP_QSTR_sensor_values
#define CLS_VALUE_QSTR   MP_QSTR_SensorValue
#define CLS_GROUP_QSTR   MP_QSTR_SensorValueGroup
#define CLS_COMP_QSTR    MP_QSTR_SensorValueComponent

// Cached class refs (loaded once)
static mp_obj_t cls_SensorValue = MP_OBJ_NULL;
static mp_obj_t cls_SensorValueGroup = MP_OBJ_NULL;
static mp_obj_t cls_SensorValueComponent = MP_OBJ_NULL;

static void ensure_classes_loaded(void) {
    if (cls_SensorValue != MP_OBJ_NULL) return;

    // import sensor_values
    mp_obj_t mod = mp_import_name(MOD_QSTR, mp_const_none, mp_const_none);
    // load classes
    cls_SensorValue         = mp_load_attr(mod, CLS_VALUE_QSTR);
    cls_SensorValueGroup    = mp_load_attr(mod, CLS_GROUP_QSTR);
    cls_SensorValueComponent= mp_load_attr(mod, CLS_COMP_QSTR);
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
    : data_scheme(data_scheme), py_callback(py_callback) {}

PipelinePythonSink::~PipelinePythonSink() {}

int PipelinePythonSink::process(const struct sensor_data *const input[], struct sensor_data *output) {
    // Call the Python callback with the processed data
    if (py_callback) {
        mp_sched_schedule(py_callback, this->parse_data(input[0]));
    }
    return 0;
}

mp_obj_t PipelinePythonSink::parse_data(const struct sensor_data *data) {
    //TODO: implement
    return mp_const_obj_none;
}
