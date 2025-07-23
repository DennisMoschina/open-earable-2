#include "status_led.h"
#include "StateIndicator.h"

mp_obj_t openearable_set_led_color(mp_obj_t r, mp_obj_t g, mp_obj_t b) {
    RGBColor color = {mp_obj_get_int(r), mp_obj_get_int(g), mp_obj_get_int(b)};
    state_indicator.set_custom_color(color);
    return mp_const_none;
}

mp_obj_t openearable_set_led_mode(mp_obj_t mode) {
    enum led_mode m = (enum led_mode)mp_obj_get_int(mode);
    state_indicator.set_indication_mode(m);
    return mp_const_none;
}