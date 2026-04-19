#pragma once

#include <vector>

#include "compute_shader.h"
#include "fluid_bounds.h"
#include "fluid_particle.h"
#include "transformable.h"

class Camera;
class Mesh;
class shader;

class gpu_fluid_system_component final : public transformable
{
    compute_shader* compute_shader_ = nullptr;
    shader* render_shader_ = nullptr;
    Mesh* render_mesh_ = nullptr;
    std::vector<fluid_particle> initial_particles_;
    fluid_bounds bounds_{};
    glm::vec3 gravity_ = glm::vec3(0.f, -9.81f, 0.f);
    float particle_size_ = 5.f;
    float interaction_radius_ = 0.42f;
    float particle_radius_ = 0.16f;
    float separation_strength_ = 0.65f;
    float near_pressure_strength_ = 0.12f;
    float velocity_damping_ = 0.35f;
    float viscosity_strength_ = 0.18f;
    float rest_density_ = 10.f;
    bool planetary_surface_enabled_ = false;
    glm::vec3 planetary_center_ = glm::vec3(0.f);
    float planetary_radius_ = 1.f;
    float planetary_shell_thickness_ = 0.1f;
    float planetary_gravity_strength_ = 9.81f;
    unsigned int solver_substeps_ = 3;
    unsigned int constraint_iterations_ = 4;
    unsigned int runtime_solver_substeps_ = 3;
    unsigned int runtime_constraint_iterations_ = 4;
    unsigned int budget_recovery_frames_ = 0;
    GLsync gpu_completion_fence_ = 0;
    float cell_size_ = 0.42f;
    bool initialized_ = false;
    size_t particle_count_ = 0;
    GLuint grid_size_x_ = 1;
    GLuint grid_size_y_ = 1;
    GLuint grid_size_z_ = 1;
    size_t cell_count_ = 1;

    static constexpr GLuint particle_binding_ = 0;
    static constexpr GLuint cell_head_binding_ = 1;
    static constexpr GLuint particle_next_binding_ = 2;

    void rebuild_grid_metadata();
    void rebuild_grid_buffers();
    void ensure_initialized();
    void release_gpu_completion_fence();
    void update_adaptive_budget();
    void queue_gpu_completion_fence();

public:
    gpu_fluid_system_component(scene_node* owner,
        compute_shader* compute_shader,
        shader* render_shader,
        Mesh* render_mesh,
        std::vector<fluid_particle> particles,
        const fluid_bounds& bounds,
        const glm::vec3& gravity = glm::vec3(0.f, -9.81f, 0.f),
        float particle_size = 5.f,
        float interaction_radius = 0.42f,
        float particle_radius = 0.16f,
        float separation_strength = 0.65f,
        float near_pressure_strength = 0.12f,
        float velocity_damping = 0.35f,
        float viscosity_strength = 0.18f,
        float rest_density = 10.f,
        unsigned int solver_substeps = 3,
        unsigned int constraint_iterations = 4);

    static type_id_t type_id();
    type_id_t get_type_id() const override;

    void attach_to(scene_node* n_node) override;
    bool detach() override;

    void fixed_update(float dt);
    void draw(Camera* camera) const;

    [[nodiscard]] size_t get_particle_count() const { return particle_count_; }
    [[nodiscard]] GLuint get_ssbo_id() const;
    void set_bounds(const fluid_bounds& bounds);
    void set_planetary_surface(const glm::vec3& center, float radius, float shell_thickness, float gravity_strength);

    ~gpu_fluid_system_component() override;
};
