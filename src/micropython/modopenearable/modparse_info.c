#include "modparse_info.h"
#include "SensorScheme.h"

static mp_obj_t build_sensor_component(const struct SensorComponent *component) {
    mp_obj_t args[3] = {
        mp_obj_new_str(component->name, strlen(component->name)),
        mp_obj_new_str(component->unit, strlen(component->unit)),
        mp_obj_new_int(component->parseType),
    };
    return mp_obj_new_tuple(3, args);
}

static mp_obj_t build_sensor_component_group(const struct SensorComponentGroup *group) {
    mp_obj_t components_list = mp_obj_new_list(0, NULL);

    for (int i = 0; i < group->componentCount; ++i) {
        mp_obj_t comp = build_sensor_component(&group->components[i]);
        mp_obj_list_append(components_list, comp);
    }

    mp_obj_t tuple[2] = {
        mp_obj_new_str(group->name, strlen(group->name)),
        components_list,
    };
    return mp_obj_new_tuple(2, tuple);
}

static mp_obj_t build_frequency_options(const struct FrequencyOptions *options) {
    mp_obj_t freq_list = mp_obj_new_list(0, NULL);
    for (int i = 0; i < options->frequencyCount; ++i) {
        mp_obj_list_append(freq_list, mp_obj_new_float(options->frequencies[i]));
    }

    mp_obj_t tuple[3] = {
        freq_list,
        mp_obj_new_int(options->defaultFrequencyIndex),
        mp_obj_new_int(options->maxBleFrequencyIndex),
    };
    return mp_obj_new_tuple(3, tuple);
}

static mp_obj_t build_sensor_scheme(const struct SensorScheme *scheme) {
    mp_obj_t group_list = mp_obj_new_list(0, NULL);
    for (int i = 0; i < scheme->groupCount; ++i) {
        mp_obj_t group = build_sensor_component_group(&scheme->groups[i]);
        mp_obj_list_append(group_list, group);
    }

    mp_obj_t config_options_type = mp_obj_new_int(scheme->configOptions.availableOptions);
    mp_obj_t config_freq_options = build_frequency_options(&scheme->configOptions.frequencyOptions);

    mp_obj_t options[2] = {
        config_options_type,
        config_freq_options,
    };

    mp_obj_t args[4] = {
        mp_obj_new_str(scheme->name, strlen(scheme->name)),
        mp_obj_new_int(scheme->id),
        group_list,
        mp_obj_new_tuple(2, options),
    };

    return mp_obj_new_tuple(4, args);
}

mp_obj_t openearable_get_sensor_schemes(void) {
    struct ParseInfoScheme* scheme = getParseInfoScheme();

    mp_obj_list_t *sensor_scheme_list = MP_OBJ_TO_PTR(mp_obj_new_list(0, NULL));

    for (int i = 0; i < scheme->sensorCount; ++i) {
        uint8_t sensor_id = scheme->sensorIds[i];
        const struct SensorScheme *sensor_scheme = getSensorSchemeForId(sensor_id);

        if (!sensor_scheme) {
            continue;
        }

        mp_obj_t scheme_obj = build_sensor_scheme(sensor_scheme);
        mp_obj_list_append(MP_OBJ_FROM_PTR(sensor_scheme_list), scheme_obj);
    }

    return MP_OBJ_FROM_PTR(sensor_scheme_list);
}