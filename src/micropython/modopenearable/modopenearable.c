#include "py/mpconfig.h"
#if MICROPY_PY_OPENEARABLE

#include "py/runtime.h"

#include "modstatus_led.h"
#include "modparse_info.h"
#include "modsensor.h"
#include "modaudiofilter.h"

static mp_obj_t openearable_info(void) {
    mp_printf(&mp_plat_print, "OpenEarable MicroPython Module\n");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(openearable_info_obj, openearable_info);

static MP_DEFINE_CONST_FUN_OBJ_3(openearable_set_led_color_obj, openearable_set_led_color);
static MP_DEFINE_CONST_FUN_OBJ_1(openearable_set_led_mode_obj, openearable_set_led_mode);
static MP_DEFINE_CONST_FUN_OBJ_0(openearable_get_sensor_schemes_obj, openearable_get_sensor_schemes);
static MP_DEFINE_CONST_FUN_OBJ_3(openearable_config_sensor_obj, openearable_config_sensor);
static MP_DEFINE_CONST_FUN_OBJ_2(openearable_on_data_received_obj, openearable_on_data_received);
static MP_DEFINE_CONST_FUN_OBJ_2(set_eq_filter_obj, set_eq_filter);
static MP_DEFINE_CONST_FUN_OBJ_2(set_anc_filter_obj, set_anc_filter);
static MP_DEFINE_CONST_FUN_OBJ_0(get_anc_sample_rate_obj, get_anc_sample_rate);
static MP_DEFINE_CONST_FUN_OBJ_0(get_eq_sample_rate_obj, get_eq_sample_rate);

static const mp_rom_map_elem_t openearable_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR__name__), MP_OBJ_NEW_QSTR(MP_QSTR__openearable) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&openearable_info_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_led_color), MP_ROM_PTR(&openearable_set_led_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_led_mode), MP_ROM_PTR(&openearable_set_led_mode_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_sensor_schemes), MP_ROM_PTR(&openearable_get_sensor_schemes_obj) },
    { MP_ROM_QSTR(MP_QSTR_config_sensor), MP_ROM_PTR(&openearable_config_sensor_obj) },
    { MP_ROM_QSTR(MP_QSTR_on_data_received), MP_ROM_PTR(&openearable_on_data_received_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_anc_filter), MP_ROM_PTR(&set_anc_filter_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_eq_filter), MP_ROM_PTR(&set_eq_filter_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_anc_sample_rate), MP_ROM_PTR(&get_anc_sample_rate_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_eq_sample_rate), MP_ROM_PTR(&get_eq_sample_rate_obj) },
};
static MP_DEFINE_CONST_DICT(openearable_module_globals, openearable_module_globals_table);

const mp_obj_module_t openearable_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&openearable_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__openearable, openearable_module);

#endif