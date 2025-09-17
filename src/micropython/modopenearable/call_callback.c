#include "call_callback.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(call_callback, LOG_LEVEL_DBG);

#include "py/obj.h"
#include "py/runtime.h"
#include "py/objlist.h"
#include "SensorScheme.h"

// ---- Per-call context passed to the scheduled thunk ----
struct cb_ctx {
    void *fifo_reserved;
    mp_obj_t cb;                        // Python callback
    const struct SensorScheme *scheme;  // owned elsewhere (sink)
    struct sensor_data sd;              // shallow copy of header
};
K_FIFO_DEFINE(cb_fifo);

// ---- Cached Python class refs (loaded once) ----
static mp_obj_t cls_SensorValue         = MP_OBJ_NULL;
static mp_obj_t cls_SensorValueGroup    = MP_OBJ_NULL;
static mp_obj_t cls_SensorValueComponent= MP_OBJ_NULL;

// If your classes live in a module named "sensor_values", set this to MP_QSTR_sensor_values.
// If they live in "sensor", use MP_QSTR_sensor.
#ifndef MOD_QSTR
#define MOD_QSTR MP_QSTR_sensor_value
#endif

static void ensure_classes_loaded(void) {
    if (cls_SensorValue != MP_OBJ_NULL) {
        return;
    }
    LOG_DBG("Importing sensor classes");
    mp_obj_t mod = mp_import_name(MOD_QSTR, mp_const_none, mp_obj_new_int(0));
    cls_SensorValue          = mp_load_attr(mod, MP_QSTR_SensorValue);
    cls_SensorValueGroup     = mp_load_attr(mod, MP_QSTR_SensorValueGroup);
    cls_SensorValueComponent = mp_load_attr(mod, MP_QSTR_SensorValueComponent);
}

// Create SensorValueComponent(name: str, value: obj, unit: str)
static mp_obj_t make_component(const char *name, mp_obj_t value_obj, const char *unit) {
    mp_obj_t args[3] = {
        mp_obj_new_str(name, (size_t)strlen(name)),
        value_obj,
        mp_obj_new_str(unit, (size_t)strlen(unit)),
    };
    return mp_call_function_n_kw(cls_SensorValueComponent, 3, 0, args);
}

// Create SensorValueGroup(name: str, components: list[SensorValueComponent])
static mp_obj_t make_group(const char *name, mp_obj_t comp_list) {
    mp_obj_t args[2] = {
        mp_obj_new_str(name, (size_t)strlen(name)),
        comp_list,
    };
    return mp_call_function_n_kw(cls_SensorValueGroup, 2, 0, args);
}

// Create SensorValue(name: str, timestamp: int, groups: list[SensorValueGroup])
static mp_obj_t make_value(const char *name, uint64_t ts, mp_obj_t group_list) {
    mp_obj_t ts_obj = mp_obj_new_int_from_ull(ts);
    mp_obj_t args[3] = {
        mp_obj_new_str(name, (size_t)strlen(name)),
        ts_obj,
        group_list,
    };
    return mp_call_function_n_kw(cls_SensorValue, 3, 0, args);
}


// Build a SensorValue Python object from a scheme + raw data (unaligned-safe)
static mp_obj_t parse_data(const struct SensorScheme *scheme, const struct sensor_data *data) {
    ensure_classes_loaded();

    // 1) list of groups
    mp_obj_t group_list = mp_obj_new_list(0, NULL);

    size_t offset = 0;
    for (size_t gi = 0; gi < scheme->groupCount; gi++) {
        const struct SensorComponentGroup *group = &scheme->groups[gi];

        // 2) list of components
        mp_obj_t comp_list = mp_obj_new_list(0, NULL);

        for (size_t cj = 0; cj < group->componentCount; cj++) {
            const struct SensorComponent *comp = &group->components[cj];
            mp_obj_t value_obj = mp_const_none;

            const size_t esz = parseTypeSizes[comp->parseType];
            if (esz == 0 || offset + esz > data->size) {
                // OOB or unsupported: emit None and stop advancing
                LOG_DBG("parse_data: type=%d size=%u offset=%u esz=%u (OOB or unsupported)",
                        (int)comp->parseType, (unsigned)data->size, (unsigned)offset, (unsigned)esz);
            } else {
                // Safe, unaligned read via memcpy into a suitably aligned local
                switch (comp->parseType) {
                    case PARSE_TYPE_UINT8: {
                        uint8_t v;  memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_int_from_uint((mp_uint_t)v);
                        break;
                    }
                    case PARSE_TYPE_INT8: {
                        int8_t v;   memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_int((mp_int_t)v);
                        break;
                    }
                    case PARSE_TYPE_UINT16: {
                        uint16_t v; memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_int_from_uint((mp_uint_t)v);
                        break;
                    }
                    case PARSE_TYPE_INT16: {
                        int16_t v;  memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_int((mp_int_t)v);
                        break;
                    }
                    case PARSE_TYPE_UINT32: {
                        uint32_t v; memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_int_from_uint((mp_uint_t)v);
                        break;
                    }
                    case PARSE_TYPE_INT32: {
                        int32_t v;  memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_int((mp_int_t)v);
                        break;
                    }
                    case PARSE_TYPE_FLOAT: {
                        float v;     memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_float((mp_float_t)v);
                        break;
                    }
                    case PARSE_TYPE_DOUBLE: {
                        double v;    memcpy(&v, data->data + offset, sizeof(v));
                        value_obj = mp_obj_new_float((mp_float_t)v); // cast to Python float
                        break;
                    }
                    default:
                        break;
                }
                offset += esz;
            }

            // 3) SensorValueComponent(name, value, unit)
            mp_obj_t comp_obj = make_component(comp->name, value_obj, comp->unit);
            mp_obj_list_append(comp_list, comp_obj);
        }

        // 4) SensorValueGroup(name, components)
        mp_obj_t group_obj = make_group(group->name, comp_list);
        mp_obj_list_append(group_list, group_obj);
    }

    // 5) SensorValue(name, timestamp, groups)
    // NOTE: adjust 'timestamp' vs 'time' to your struct!
    mp_obj_t sv_obj = make_value(scheme->name,
                                 (uint64_t)data->time /* or data->time */,
                                 group_list);
    return sv_obj;
}

// Scheduled thunk: arg carries a pointer to cb_ctx as an integer object
static mp_obj_t call_cb_py(mp_obj_t arg) {
    ARG_UNUSED(arg);

    struct cb_ctx *ctx = k_fifo_get(&cb_fifo, K_NO_WAIT);
    if (!ctx) {
        LOG_ERR("call_cb_py: no context");
        return mp_const_none;
    }
    mp_obj_t d = parse_data(ctx->scheme, &ctx->sd);
    mp_call_function_1(ctx->cb, d);

    k_free(ctx);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(call_cb_py_obj, call_cb_py);

void call_callback(mp_obj_t callback,
                   const struct sensor_data *data,
                   const struct SensorScheme *scheme)
{
    struct cb_ctx *ctx = (struct cb_ctx *)k_malloc(sizeof(struct cb_ctx));
    if (!ctx) {
        LOG_ERR("ctx alloc failed");
        return;
    }

    ctx->cb     = callback;
    ctx->scheme = scheme;

    // Copy header first
    ctx->sd = *data;

    // Copy payload into embedded array (no pointer reassignment!)
    // Clamp to the fixed capacity of the destination buffer.
    size_t cap = sizeof(ctx->sd.data);                // SENSOR_DATA_FIXED_LENGTH
    size_t n   = (data->size <= cap) ? data->size : cap;
    if (n > 0) {
        memcpy(ctx->sd.data, data->data, n);
    }
    ctx->sd.size = n;

    k_fifo_put(&cb_fifo, ctx);
    mp_sched_schedule(MP_OBJ_FROM_PTR(&call_cb_py_obj), mp_const_none);
}
