#pragma once

#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"

class compute_shader;

class fluid_scene final : public scene
{
    shader* grid_shader_ = nullptr;
    Mesh* grid_mesh_ = nullptr;
    shader* fluid_shader_ = nullptr;
    Mesh* fluid_mesh_ = nullptr;
    compute_shader* fluid_compute_shader_ = nullptr;
    scene_node* fluid_node_ = nullptr;
    sim::time* sim_time_ = nullptr;
    bool previous_debug_next_down_ = false;
    bool previous_debug_prev_down_ = false;
    gpu_fluid_system_component* fluid_system_ = nullptr;

    void initialize_scene_content();

public:
    explicit fluid_scene(sim::time* time);
    void handle_input(engine& engine, float dt) override;
    void update() override;
    ~fluid_scene() override = default;
};
