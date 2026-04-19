#pragma once

#include <memory>

#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"

class compute_shader;

class cloth_scene final : public scene
{
    std::unique_ptr<shader> grid_shader_;
    std::unique_ptr<Mesh> grid_mesh_;
    std::unique_ptr<shader> cloth_particle_shader_;
    std::unique_ptr<shader> cloth_link_shader_;
    std::unique_ptr<Mesh> cloth_particle_mesh_;
    std::unique_ptr<Mesh> cloth_link_mesh_;
    std::unique_ptr<compute_shader> cloth_compute_shader_;

    void initialize_scene_content();

public:
    explicit cloth_scene(sim::time* time);
    ~cloth_scene() override = default;
};
