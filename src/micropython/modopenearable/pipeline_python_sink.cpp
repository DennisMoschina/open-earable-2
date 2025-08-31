#include "pipeline_python_sink.h"

#include <stdlib.h>
// sensor_bridge.c
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objlist.h"
#include "py/binary.h"

#include "ParseType.h"

#include "call_callback.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openearable_pipeline_python_sink, LOG_LEVEL_DBG);


PipelinePythonSink::PipelinePythonSink(const struct SensorScheme &data_scheme, mp_obj_t py_callback)
    : SensorProcessingStage(1), data_scheme(data_scheme), py_callback(py_callback) {}

PipelinePythonSink::~PipelinePythonSink() {
    free((void*)data_scheme.name);
    for (size_t gi = 0; gi < data_scheme.groupCount; ++gi) {
        auto &grp = data_scheme.groups[gi];
        free((void*)grp.name);
        for (size_t ci = 0; ci < grp.componentCount; ++ci) {
            free((void*)grp.components[ci].name);
            free((void*)grp.components[ci].unit);
        }
        delete[] grp.components;
    }
    delete[] data_scheme.groups;
}

int PipelinePythonSink::process(const struct sensor_data *const inputs[], struct sensor_data *output) {
    call_callback(this->py_callback, inputs[0], &data_scheme);
    return 0;
}
