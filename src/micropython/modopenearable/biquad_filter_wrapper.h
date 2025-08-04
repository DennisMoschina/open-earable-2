#ifndef BIQUAD_FILTER_WRAPPER_H
#define BIQUAD_FILTER_WRAPPER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void* BiQuadHandle;

BiQuadHandle biquad_filter_create(int num_stages);
void biquad_filter_destroy(BiQuadHandle handle);

#ifdef __cplusplus
}
#endif

#endif // BIQUAD_FILTER_WRAPPER_H