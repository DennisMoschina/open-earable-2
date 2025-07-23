#ifndef STATUS_LED_H
#define STATUS_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <py/runtime.h>

mp_obj_t openearable_set_led_color(mp_obj_t r, mp_obj_t g, mp_obj_t b);
mp_obj_t openearable_set_led_mode(mp_obj_t mode);

#ifdef __cplusplus
}
#endif

#endif // STATUS_LED_H