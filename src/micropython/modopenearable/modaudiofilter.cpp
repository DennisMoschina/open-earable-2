#include "modaudiofilter.h"

#include <py/objarray.h>
#include <py/runtime.h>

#include <ADAU1860.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mod_audio_filter, LOG_LEVEL_DBG);

uint32_t* cast_coefficients(mp_obj_t coeffs) {
    mp_obj_iter_buf_t iter_buf;
    mp_obj_t item, iterable = mp_getiter(coeffs, &iter_buf);
    size_t count = 0;
    static uint32_t coefficients[5];
    while ((item = mp_iternext(iterable)) != MP_OBJ_STOP_ITERATION) {
        if (count >= 5) {
            mp_raise_ValueError("Too many coefficients provided");
        }
        if (!mp_obj_is_int(item)) {
            mp_raise_TypeError("Coefficients must be integers");
        }
        coefficients[count++] = (uint32_t)mp_obj_get_int(item);
    }
    if (count < 5) {
        mp_raise_ValueError("Too few coefficients provided, expected 5");
    }

    return coefficients;
}

mp_obj_t set_anc_filter(mp_obj_t filter_slot, mp_obj_t coeffs) {
    uint32_t* coefficients = cast_coefficients(coeffs);
    if (coefficients == NULL) {
        mp_raise_ValueError("Invalid coefficients provided");
    }
    const int filter_slot_value = mp_obj_get_int(filter_slot);

    LOG_DBG("Setting ANC filter on slot %d with coefficients: %u, %u, %u, %u, %u",
           filter_slot_value,
           coefficients[0], coefficients[1], coefficients[2], coefficients[3], coefficients[4]);

    enum sl_address address;
    switch (filter_slot_value) {
    case BIQ_0:
        address = BIQ_0;
        break;
    case EXPANDER:
        address = EXPANDER;
        break;
    case VOLUME:
        address = VOLUME;
        break;
    case MUTE:
        address = MUTE;
        break;
    case MIXER:
        address = MIXER;
        break;
    case LIMITER_MASTER:
        address = LIMITER_MASTER;
        break;
    default:
        mp_raise_ValueError("Invalid filter slot specified");
    }

    dac.fdsp_safe_load(address, coefficients, true);

    return mp_const_none;
}

mp_obj_t set_eq_filter(mp_obj_t filter_slot, mp_obj_t coeffs) {
    uint32_t* coefficients = cast_coefficients(coeffs);
    if (coefficients == NULL) {
        mp_raise_ValueError("Invalid coefficients provided");
    }
    const int filter_slot_value = mp_obj_get_int(filter_slot);
    // Implementation for setting EQ filter using coefficients
    // This would typically involve writing to a specific register or memory location
    // in the audio processing unit.

    LOG_DBG("Setting EQ filter on slot %d with coefficients: %u, %u, %u, %u, %u",
           filter_slot_value,
           coefficients[0], coefficients[1], coefficients[2], coefficients[3], coefficients[4]);
    
    // TODO: implement

    mp_raise_NotImplementedError("EQ filter setting not implemented yet");

    return mp_const_none;
}

mp_obj_t get_anc_sample_rate(void) {
    // TODO: make this dynamic based on the actual ANC configuration
    return mp_obj_new_int(192000);
}
mp_obj_t get_eq_sample_rate(void) {
    // TODO: make this dynamic based on the actual EQ configuration
    return mp_obj_new_int(48000);
}
