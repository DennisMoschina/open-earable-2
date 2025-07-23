#include "py/mpconfig.h"
#if MICROPY_PY_OPENEARABLE

#include "py/runtime.h"

#include "status_led.h"

static mp_obj_t openearable_info(void) {
    mp_printf(&mp_plat_print, "OpenEarable MicroPython Module\n");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(openearable_info_obj, openearable_info);

static MP_DEFINE_CONST_FUN_OBJ_3(openearable_set_led_color_obj, openearable_set_led_color);
static MP_DEFINE_CONST_FUN_OBJ_1(openearable_set_led_mode_obj, openearable_set_led_mode);

static const mp_rom_map_elem_t openearable_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR__name__), MP_OBJ_NEW_QSTR(MP_QSTR__openearable) },
    { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&openearable_info_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_led_color), MP_ROM_PTR(&openearable_set_led_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_led_mode), MP_ROM_PTR(&openearable_set_led_mode_obj) },
};
static MP_DEFINE_CONST_DICT(openearable_module_globals, openearable_module_globals_table);

const mp_obj_module_t openearable_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&openearable_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__openearable, openearable_module);

#endif