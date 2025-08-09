#include "sensor_value.h"
#include "SensorComponent.h"
#include "SensorScheme.h"
#include "openearable_common.h"
#include <stdlib.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_parser, LOG_LEVEL_DBG);

// Internal cache per scheme
typedef struct {
    bool                  init;
    sensor_value_t        sv;
    sensor_value_group_t *groups;
    sensor_value_component_t *components;
    size_t                *component_offsets;
    size_t                total_components;
} SVCache;

static SVCache *g_sv_cache_by_id[256];

// Helpers ---------------------------------------------------------------------
static inline int8_t rd_i8_le(const uint8_t *p) {
    return (int8_t)p[0];
}
static inline uint8_t rd_u8_le(const uint8_t *p) {
    return (uint8_t)p[0];
}
static inline uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline int16_t rd_i16_le(const uint8_t *p) {
    return (int16_t)rd_u16_le(p);
}
static inline uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)rd_u16_le(p) | ((uint32_t)rd_u16_le(p+2) << 16);
}
static inline int32_t rd_i32_le(const uint8_t *p) {
    return (int32_t)rd_u32_le(p);
}
static inline uint64_t rd_u64_le(const uint8_t *p) {
    return (uint64_t)rd_u32_le(p) | ((uint64_t)rd_u32_le(p+4) << 32);
}
static inline float rd_f32_le(const uint8_t *p) {
    float v;
    memcpy(&v, p, 4);
    return v;
}
static inline double rd_f64_le(const uint8_t *p) {
    double v;
    memcpy(&v, p, 8);
    return v;
}

static inline size_t parse_type_size(enum ParseType t){
    return parseTypeSizes[t];
}

// Build (once) or return the cached prototype for this scheme ID.
static SVCache *get_or_build_cache(struct SensorScheme *scheme) {
    if (!scheme) {
        LOG_ERR("Invalid scheme pointer");
        return NULL;
    }

    SVCache **slot = &g_sv_cache_by_id[scheme->id];

    // Fast path: already built.
    if (*slot) {
        // scheme->sv_cache = *slot;  // keep in sync (optional)
        return *slot;
    }

    // allocate cache root
    SVCache *c = (SVCache *)calloc(1, sizeof(SVCache));
    if (!c) {
        LOG_ERR("Failed to allocate cache");
        return NULL;
    }

    c->sv.name        = scheme->name;
    c->sv.group_count = scheme->groupCount;

    // allocate groups array
    c->groups = (sensor_value_group_t *)calloc(scheme->groupCount, sizeof(sensor_value_group_t));
    if (!c->groups) {
        free(c);
        LOG_ERR("Failed to allocate groups");
        return NULL;
    }

    // count total components across all groups
    size_t total = 0;
    for (size_t g = 0; g < scheme->groupCount; ++g) {
        total += scheme->groups[g].componentCount;
    }
    c->total_components = total;

    // allocate flat component arena
    c->components = (sensor_value_component_t *)calloc(total, sizeof(sensor_value_component_t));
    if (!c->components) {
        free(c->groups);
        free(c);
        LOG_ERR("Failed to allocate components");
        return NULL;
    }

    c->component_offsets = (size_t *)calloc(scheme->groupCount, sizeof(size_t));
    if (!c->component_offsets) {
        free(c->components);
        free(c->groups);
        free(c);
        LOG_ERR("Failed to allocate component offsets");
        return NULL;
    }

    // wire groups/components, copy immutable metadata from scheme
    size_t comp_idx = 0;
    size_t comp_offset = 0;
    for (size_t g = 0; g < scheme->groupCount; ++g) {
        const struct SensorComponentGroup *sg = &scheme->groups[g];
        sensor_value_group_t *gg = &c->groups[g];

        gg->name            = sg->name;
        gg->component_count = sg->componentCount;
        gg->components      = &c->components[comp_idx];

        for (size_t i = 0; i < sg->componentCount; ++i, ++comp_idx) {
            const struct SensorComponent *sc = &sg->components[i];
            sensor_value_component_t *cc   = &gg->components[i];

            cc->name       = sc->name;        // assumes lifetime ≥ cache
            cc->unit       = sc->unit;        // assumes lifetime ≥ cache
            cc->parse_type = sc->parseType;  // immutable
            // cc->value stays zero-initialized; overwritten on parse

            c->component_offsets[comp_idx] = comp_offset;
            comp_offset += parse_type_size(cc->parse_type);
        }
    }

    c->sv.groups = c->groups;
    c->init = true;

    // publish
    *slot = c;
    return c;
}

/**
 * @brief Parse the value of a sensor component from a byte buffer.
 *
 * @param cc Pointer to the sensor_value_component_t structure to fill.
 * @param sv_cache Pointer to the SVCache containing offsets and metadata.
 * @param comp_index Index of the component in the cache.
 * @param buffer Pointer to the byte buffer containing the raw data.
 */
static void parse_component_value(sensor_value_component_t *cc,
                                  const SVCache *sv_cache,
                                  size_t comp_index,
                                  const uint8_t *buffer)
{
    const uint8_t *p;

    p = buffer + sv_cache->component_offsets[comp_index];

    switch (cc->parse_type) {
        case PARSE_TYPE_INT8: cc->value.i8 = rd_i8_le(p); break;
        case PARSE_TYPE_UINT8: cc->value.u8 = rd_u8_le(p); break;
        case PARSE_TYPE_INT16: cc->value.i16 = rd_i16_le(p); break;
        case PARSE_TYPE_UINT16: cc->value.u16 = rd_u16_le(p); break;
        case PARSE_TYPE_INT32: cc->value.i32 = rd_i32_le(p); break;
        case PARSE_TYPE_UINT32: cc->value.u32 = rd_u32_le(p); break;
        case PARSE_TYPE_FLOAT: cc->value.f32 = rd_f32_le(p); break;
        case PARSE_TYPE_DOUBLE: cc->value.f64 = rd_f64_le(p); break;
        default: break;
    }
}

// Public API ------------------------------------------------------------------
// NOTE: returns a struct that references memory owned by the scheme cache
// (when cache=true). Valid until next parse on the same scheme or clear.
sensor_value_t parse_sensor_value(struct sensor_data sensor_data,
                                  struct SensorScheme *scheme,
                                  bool cache /* = true */)
{
    SVCache *c = get_or_build_cache(scheme);
    // If cache allocation failed, return an empty object
    if (!c) {
        sensor_value_t empty = {0};
        return empty;
    }

    // Overwrite only the *values* and timestamp
    size_t comp_idx = 0;
    for (size_t g = 0; g < scheme->groupCount; ++g) {
        const struct SensorComponentGroup *sg = &scheme->groups[g];
        sensor_value_group_t *gg = &c->groups[g];

        for (size_t i = 0; i < sg->componentCount; ++i) {
            sensor_value_component_t *cc = &gg->components[i];

            parse_component_value(cc, c, comp_idx, sensor_data.data);
            comp_idx++;
        }
    }

    // Timestamp strategy: if your payload includes ts, just map it as a component.
    // Otherwise set here (e.g., host time in µs or read from scheme hook).
    c->sv.timestamp = sensor_data.time;

    if (cache) {
        // Return the cached handle (struct by value, but it points into cache)
        return c->sv;
    } else {
        // Deep copy the current snapshot (rare path)
        sensor_value_t out = {0};
        out.name        = c->sv.name;
        out.group_count = c->sv.group_count;
        out.timestamp   = c->sv.timestamp;

        // allocate fresh groups/components
        out.groups = (sensor_value_group_t*)calloc(out.group_count, sizeof(sensor_value_group_t));
        if (!out.groups) return (sensor_value_t){0};

        size_t total = 0;
        for (size_t g = 0; g < out.group_count; ++g) total += scheme->groups[g].componentCount;

        sensor_value_component_t *comps = (sensor_value_component_t*)calloc(total, sizeof(sensor_value_component_t));
        if (!comps) { free(out.groups); return (sensor_value_t){0}; }

        size_t off = 0;
        for (size_t g = 0; g < out.group_count; ++g) {
            const struct SensorComponentGroup *sg = &scheme->groups[g];
            sensor_value_group_t *gg = &out.groups[g];
            gg->name = sg->name;
            gg->component_count = sg->componentCount;
            gg->components = &comps[off];

            for (size_t i = 0; i < sg->componentCount; ++i) {
                const sensor_value_component_t *src = &c->groups[g].components[i];
                sensor_value_component_t *dst = &gg->components[i];
                *dst = *src; // copies value + parse_type + pointers to names/units
            }
            off += sg->componentCount;
        }
        return out;
    }
}

void clear_sensor_value_cache(struct SensorScheme *scheme) {
    if (!scheme) return;
    SVCache **slot = &g_sv_cache_by_id[scheme->id];
    SVCache *c = *slot;
    if (!c) return;

    free(c->components);
    free(c->groups);
    free(c);
    *slot = NULL;
}

void clear_all_sensor_value_caches(void) {
    for (int i = 0; i < 256; ++i) {
        if (g_sv_cache_by_id[i]) {
            SVCache *c = g_sv_cache_by_id[i];
            free(c->components);
            free(c->groups);
            free(c);
            g_sv_cache_by_id[i] = NULL;
        }
    }
}