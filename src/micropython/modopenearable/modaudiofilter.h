#ifndef _AUDIO_FILTER_H
#define _AUDIO_FILTER_H


#ifdef __cplusplus
extern "C" {
#endif

#include <py/runtime.h>

mp_obj_t set_anc_filter(mp_obj_t filter_slot, mp_obj_t coeffs);
mp_obj_t set_eq_filter(mp_obj_t filter_slot, mp_obj_t coeffs);

mp_obj_t get_anc_sample_rate(void);
mp_obj_t get_eq_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* _AUDIO_FILTER_H */