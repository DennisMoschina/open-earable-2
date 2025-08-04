#include "py/runtime.h"
#include "py/obj.h"
#include "py/objarray.h"

#include "biquad_filter_wrapper.h"
#include "BiQuadFilter.h"

extern "C" {

typedef void* BiquadHandle;

BiquadHandle biquad_filter_create(int stages) {
    return new BiQuadFilter(stages);
}

void biquad_filter_apply(BiquadHandle handle, int16_t* data, int length) {
    static_cast<BiQuadFilter*>(handle)->apply(data, length);
}

void biquad_filter_destroy(BiquadHandle handle) {
    delete static_cast<BiQuadFilter*>(handle);
}

}