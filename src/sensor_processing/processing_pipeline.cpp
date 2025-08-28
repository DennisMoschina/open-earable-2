#include "processing_pipeline.h"

#include <zephyr/logging/log.h>
#include <memory>
#include "sensor_source_stage.h"
#include <queue>
LOG_MODULE_REGISTER(processing_pipeline, LOG_LEVEL_DBG);

ProcessingPipeline::ProcessingPipeline() {
    // Initialize the processing pipeline
}

void ProcessingPipeline::add_source(const char *name, std::unique_ptr<SensorProcessingStage> source) {
    source_map[source->get_in_ports()].push_back(stages.size());
    stages.push_back({name, std::move(source), {}, {}});
}

void ProcessingPipeline::add_stage(const char *name, std::unique_ptr<SensorProcessingStage> stage) {
    stages.push_back({name, std::move(stage), {}, {}});
}

//TODO: handle errors
void ProcessingPipeline::connect(size_t src, size_t dst, size_t dst_port) {
    if (src < stages.size() && dst < stages.size()) {
        stages[dst].inputs.push_back({src, dst_port});
    }
}

void ProcessingPipeline::connect(const char* src, const char *dest, size_t dst_port) {
    size_t src_idx = stages.size();
    size_t dst_idx = stages.size();
    for (size_t i = 0; i < stages.size(); ++i) {
        if (strcmp(stages[i].name, src) == 0) {
            src_idx = i;
        }
        if (strcmp(stages[i].name, dest) == 0) {
            dst_idx = i;
        }
    }
    this->connect(src_idx, dst_idx, dst_port);
}

int ProcessingPipeline::inject(const struct sensor_data& sample) {
    int ret = 0;
    // Find source stage with matching id
    for (size_t src_idx : source_map[sample.id]) {
        stages[src_idx].output = sample;
        stages[src_idx].has_output = true;

        // Execute downstream stages starting from this node
        ret = run_from(src_idx);
        if (ret < 0) {
            LOG_ERR("Failed to run from source stage: %d", ret);
            return ret;
        }
    }

    return -EINVAL; // no matching source found
}

int ProcessingPipeline::run() {
    return run_from(0);
}

int ProcessingPipeline::run_from(size_t node_index) {
    for (size_t i = node_index; i < stages.size(); ++i) {
        PipelineNode& node = stages[i];
        size_t in_count = node.stage->get_in_ports();
        std::vector<const sensor_data*> inputs(in_count);

        bool ready = true;

        // resolve upstream outputs
        for (size_t port = 0; port < in_count; ++port) {
            Edge& e = node.inputs[port];
            if (!stages[e.src].has_output) {
                ready = false;
                break;
            }
            inputs[port] = &stages[e.src].output;
        }

        if (!ready) {
            // not ready to process
            continue;
        }

        // call stage
        int rc = node.stage->process(inputs.data(), &node.output);

        if (rc < 0) {
            // error case
            return rc;
        } else if (rc > 0) {
            // no new result: mark node as "inactive"
            node.has_output = false;
            continue;
        } else {
            // valid result
            node.has_output = true;
        }
    }
    return 0;
}

const struct sensor_data& ProcessingPipeline::get_output(size_t node_index) const {
    return stages[node_index].output;
}
