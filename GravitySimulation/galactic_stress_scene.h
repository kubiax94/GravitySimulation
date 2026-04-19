#pragma once

#include <memory>
#include <vector>

#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"

class renderer;

class galactic_stress_scene final : public scene
{
    std::vector<renderer*> stress_renderers_;
    std::unique_ptr<shader> grid_shader_;
    std::unique_ptr<Mesh> grid_mesh_;
    std::unique_ptr<shader> sun_shader_;
    std::unique_ptr<Mesh> sun_mesh_;

    void initialize_scene_content();

public:
    explicit galactic_stress_scene(sim::time* time);
    ~galactic_stress_scene() override = default;
};
