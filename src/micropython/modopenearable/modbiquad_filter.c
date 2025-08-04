#include "py/obj.h"
#include "py/runtime.h"

#include "biquad_filter_wrapper.h"


typedef struct _biquad_filter_obj_t {
    mp_obj_base_t base;
    BiQuadHandle filter;
} biquad_filter_obj_t;

mp_obj_t biquad_filter_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    biquad_filter_obj_t *self = m_new_obj(biquad_filter_obj_t);
    self->filter = biquad_filter_create(mp_obj_get_int(args[0]));

    return MP_OBJ_FROM_PTR(self);
}

static const mp_rom_map_elem_t biquad_filter_locals_dict_table[] = {

};
static MP_DEFINE_CONST_DICT(biquad_filter_locals_dict, biquad_filter_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    biquad_filter_type,
    MP_QSTR_BiQuadFilter,
    MP_TYPE_FLAG_NONE,
    make_new, (const void *)biquad_filter_make_new,
    locals_dict, &biquad_filter_locals_dict
);

static const mp_rom_map_elem_t biquad_filter_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR__name__), MP_OBJ_NEW_QSTR(MP_QSTR_biquad_filter) },
    { MP_ROM_QSTR(MP_QSTR_BiQuadFilter), MP_ROM_PTR(&biquad_filter_type) },
};
static MP_DEFINE_CONST_DICT(biquad_filter_module_globals, biquad_filter_module_globals_table);


const mp_obj_module_t biquad_filter_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&biquad_filter_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_biquad_filter, biquad_filter_module);
