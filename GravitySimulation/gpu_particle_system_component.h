#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "compute_shader.h"
#include "physics_data.h"
#include "transformable.h"
#include "unit_system.h"

class Camera;
class Mesh;
class shader;

class gpu_particle_system_component final : public transformable
{
    compute_shader* compute_shader_ = nullptr;
    shader* render_shader_ = nullptr;
    Mesh* render_mesh_ = nullptr;
    unit_system* unit_system_ = nullptr;
    std::vector<physics_data> initial_particles_;
    float simulation_time_ = 0.f;
    float particle_size_ = 3.5f;
    float simulation_speed_ = 20.0f;
    glm::vec3 particle_color_ = glm::vec3(1.0f, 0.55f, 0.18f);
    float particle_alpha_ = 1.0f;
    float particle_glow_strength_ = 1.0f;
    float particle_size_jitter_ = 0.0f;
    int particle_visual_mode_ = 0;
    bool additive_blend_enabled_ = false;
    bool depth_write_enabled_ = true;
    bool initialized_ = false;
    size_t particle_count_ = 0;

    static constexpr GLuint physics_binding_ = 0;

    void ensure_initialized();

public:
    gpu_particle_system_component(scene_node* owner,
        compute_shader* compute_shader,
        shader* render_shader,
        Mesh* render_mesh,
        unit_system* unit_system,
        std::vector<physics_data> particles,
        float particle_size = 3.5f,
        float simulation_speed = 20.0f);

    static type_id_t type_id();
    type_id_t get_type_id() const override;

    void attach_to(scene_node* n_node) override;
    bool detach() override;

    void fixed_update(float dt);
    void draw(Camera* camera) const;

    [[nodiscard]] size_t get_particle_count() const { return particle_count_; }
    [[nodiscard]] GLuint get_ssbo_id() const;
    void set_particle_color(const glm::vec3& color) { particle_color_ = color; }
    void set_particle_alpha(float alpha) { particle_alpha_ = alpha; }
    void set_particle_glow_strength(float glow_strength) { particle_glow_strength_ = glow_strength; }
    void set_particle_size_jitter(float jitter) { particle_size_jitter_ = jitter; }
    void set_particle_visual_mode(int mode) { particle_visual_mode_ = mode; }
    void set_additive_blend_enabled(bool enabled) { additive_blend_enabled_ = enabled; }
    void set_depth_write_enabled(bool enabled) { depth_write_enabled_ = enabled; }
};
