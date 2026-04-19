#pragma once

#include <vector>

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
};
