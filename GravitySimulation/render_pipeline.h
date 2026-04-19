#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "Renderer.h"
#include "instance_manager.h"

class scene;

struct render_item {
    renderer* render = nullptr;
};

class render_pipeline
{
    struct batch_key {
        shader* shader_ptr{};
        Mesh* mesh_ptr{};

        bool operator==(const batch_key& other) const {
            return shader_ptr == other.shader_ptr && mesh_ptr == other.mesh_ptr;
        }
    };

    struct batch_key_hash {
        size_t operator()(const batch_key& key) const {
            return std::hash<void*>{}(key.shader_ptr) ^ (std::hash<void*>{}(key.mesh_ptr) << 1);
        }
    };

    struct cached_batch {
        batch_key key;
        std::vector<renderer*> renders;
        bool uses_gpu_positions = false;
        std::vector<uint64_t> instance_revisions;
        std::vector<glm::mat4> instance_models;
    };

    std::vector<render_item> items_;
    std::vector<renderer*> cached_submission_;
    std::vector<cached_batch> cached_batches_;
    instance_manager instance_manager_;

    [[nodiscard]] static bool is_render_valid(const renderer* render);
    void rebuild_cached_batches();
    [[nodiscard]] bool can_reuse_cached_batches() const;
    void update_cached_batch_instances(cached_batch& batch, bool use_gpu_positions) const;

public:
    void begin_frame();
    void submit(renderer* render);
    void submit(const render_item& item);
    void flush(Camera* camera, const scene* scene_context, const std::function<void(shader&)>& pre_draw = nullptr);
};