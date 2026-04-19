#pragma once

#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"

class cloth_scene final : public scene
{
   shader* grid_shader_ = nullptr;
    Mesh* grid_mesh_ = nullptr;
    shader* cloth_particle_shader_ = nullptr;
    shader* cloth_link_shader_ = nullptr;
    Mesh* cloth_particle_mesh_ = nullptr;
    Mesh* cloth_link_mesh_ = nullptr;
    compute_shader* cloth_compute_shader_ = nullptr;

    void initialize_scene_content();

public:
    explicit cloth_scene(sim::time* time);
    ~cloth_scene() override = default;
};
