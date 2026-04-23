#pragma once

#include <vector>

#include "Scene.h"
#include "Renderer.h"

class galactic_scene final : public scene
{
    std::vector<renderer*> planet_renderers_;
    scene_node* camera_node_ = nullptr;
    scene_node* background_star_node_ = nullptr;
    scene_node* background_galaxy_node_ = nullptr;

    void initialize_scene_content();

public:
    explicit galactic_scene(sim::time* time);
    void update() override;
    [[nodiscard]] bool has_primary_light() const override { return true; }
    [[nodiscard]] glm::vec3 get_primary_light_position() const override { return glm::vec3(0.f); }
    [[nodiscard]] glm::vec3 get_primary_light_color() const override { return glm::vec3(1.0f, 0.82f, 0.45f); }
    [[nodiscard]] float get_primary_light_intensity() const override { return 1.2f; }
    ~galactic_scene() override = default;
};
