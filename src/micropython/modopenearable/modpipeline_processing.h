#ifndef _MOD_PROCESSING_SINK_H
#define _MOD_PROCESSING_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py/runtime.h"

mp_obj_t openearable_create_processing_pipeline(mp_obj_t name);
mp_obj_t openearable_remove_processing_pipeline(mp_obj_t name);
mp_obj_t openearable_processing_pipeline_add_stage(mp_obj_t pipeline_name, mp_obj_t stage_name, mp_obj_t stage);
mp_obj_t openearable_connect_stages(size_t n_args, const mp_obj_t *args);

#ifdef __cplusplus
}
#endif

#endif // _MOD_PROCESSING_SINK_H