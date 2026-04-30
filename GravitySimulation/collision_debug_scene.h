#pragma once

#include "Scene.h"

class Mesh;
class shader;

class collision_debug_scene final : public scene
{
    sim::time* debug_time_ = nullptr;
    compute_shader* debug_gravity_compute_shader_ = nullptr;
    Mesh* debug_cube_mesh_ = nullptr;
    shader* debug_cube_shader_ = nullptr;
    bounding_box debug_cube_bounds_{};
    int dynamic_spawn_serial_ = 0;
    float next_wave_spawn_time_ = 0.5f;
    float next_crusher_spawn_time_ = 0.75f;

    void initialize_scene_content();
    scene_node* create_static_debug_box(const std::string& name, const glm::vec3& position, const glm::vec3& scale, collision_layer layer);
    scene_node* create_dynamic_debug_box(const std::string& name, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& velocity, float mass, collision_layer layer);
    void spawn_collision_wave();
    void spawn_crusher_wave();

public:
    explicit collision_debug_scene(sim::time* time);
    void update() override;
    ~collision_debug_scene() override = default;
};
