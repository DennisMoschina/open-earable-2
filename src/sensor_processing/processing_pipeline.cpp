#include "processing_pipeline.h"

#include <zephyr/logging/log.h>
#include <memory>
#include "sensor_source_stage.h"
#include <queue>
LOG_MODULE_REGISTER(processing_pipeline, LOG_LEVEL_DBG);

ProcessingPipeline::ProcessingPipeline() {
    // Initialize the source_map
    this->source_map = std::map<uint8_t, std::vector<size_t>>{};
    this->stages = std::vector<PipelineNode>{};
}

void ProcessingPipeline::add_source(const char *name, std::unique_ptr<SensorSourceStage> source) {
    uint8_t sensor_id = source->get_sensor_id();

    // check if source_map already has an entry for this sensor ID
    if (this->source_map.find(sensor_id) == this->source_map.end()) {
        LOG_DBG("Registering new source ID %d", sensor_id);
        this->source_map[sensor_id] = {};
    }

    this->source_map[sensor_id].push_back(this->stages.size());
    this->stages.push_back({name, std::move(source), {}, {}});
    LOG_DBG("Added source %s for sensor ID %d", name, sensor_id);
}
    
void ProcessingPipeline::add_stage(const char *name, std::unique_ptr<SensorProcessingStage> stage) {
    stages.push_back({name, std::move(stage), {}, {}});
    LOG_DBG("Added stage %s", name);
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
    auto it = source_map.find(sample.id);
    if (it == source_map.end() || it->second.empty()) {
        return -EINVAL; // no matching source
    }

    for (size_t src_idx : it->second) {
        // Seed source output
        stages[src_idx].output     = sample;
        stages[src_idx].has_output = true;

        // Propagate from this source
        int rc = run_from(src_idx);
        if (rc < 0) return rc;
    }
    return 0;
}

int ProcessingPipeline::run() {
    return run_from(0);
}

int ProcessingPipeline::run_from(size_t node_index) {
    LOG_DBG("Running pipeline from node %d", node_index);
    for (size_t i = node_index; i < this->stages.size(); ++i) {
        PipelineNode& node = stages[i];
        size_t in_count = node.stage->get_in_ports();
        if (in_count == 0) {
            LOG_DBG("Node %s has no inputs, skipping", node.name);
            continue;
        }
        std::vector<const sensor_data*> inputs(in_count);

        bool ready = true;

        // resolve upstream outputs
        if (node.inputs.size() < in_count) {
            LOG_ERR("Node %s: expected %zu inputs, but only %zu edges connected",
                    node.name, in_count, node.inputs.size());
            ready = false;
        } else {
            for (size_t port = 0; port < in_count; ++port) {
                Edge& e = node.inputs[port];
                LOG_DBG("Node %s: resolving input %zu from node %zu, port %zu",
                        node.name, port, e.src, e.dstPort);
                if (!stages[e.src].has_output) {
                    ready = false;
                    break;
                }
                inputs[port] = &stages[e.src].output;
            }
        }

        if (!ready) {
            // not ready to process
            LOG_DBG("Node %s not ready, skipping", node.name);
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
