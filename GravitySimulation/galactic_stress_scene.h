#pragma once

#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"

class compute_shader;

class galactic_stress_scene final : public scene
{
   shader* grid_shader_ = nullptr;
    Mesh* grid_mesh_ = nullptr;
    shader* sun_shader_ = nullptr;
    Mesh* sun_mesh_ = nullptr;
    shader* particle_shader_ = nullptr;
    Mesh* particle_mesh_ = nullptr;
    compute_shader* particle_compute_shader_ = nullptr;

    void initialize_scene_content();

public:
    explicit galactic_stress_scene(sim::time* time);
    ~galactic_stress_scene() override = default;
};
