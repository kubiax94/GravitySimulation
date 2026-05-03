#pragma once

#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"

#include <vector>

class compute_shader;

class galactic_stress_scene final : public scene
{
   shader* grid_shader_ = nullptr;
    Mesh* grid_mesh_ = nullptr;
    shader* sun_shader_ = nullptr;
    Mesh* sun_mesh_ = nullptr;
    std::vector<renderer*> planet_renderers_;

    void initialize_scene_content();

public:
    explicit galactic_stress_scene(sim::time* time);
    [[nodiscard]] bool has_primary_light() const override { return true; }
    [[nodiscard]] glm::vec3 get_primary_light_position() const override { return glm::vec3(0.f); }
    [[nodiscard]] glm::vec3 get_primary_light_color() const override { return glm::vec3(1.0f, 0.82f, 0.45f); }
    [[nodiscard]] float get_primary_light_intensity() const override { return 1.2f; }
    ~galactic_stress_scene() override = default;
};
