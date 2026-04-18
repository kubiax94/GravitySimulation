#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "Renderer.h"
#include "instance_manager.h"

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

    std::vector<render_item> items_;
    instance_manager instance_manager_;

public:
    void begin_frame();
    void submit(renderer* render);
    void submit(const render_item& item);
    void flush(Camera* camera, const std::function<void(shader&)>& pre_draw = nullptr);
};