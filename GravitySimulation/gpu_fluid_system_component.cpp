#include "gpu_fluid_system_component.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>

#include "Camera.h"
#include "Mesh.h"
#include "Shader.h"

namespace {
constexpr unsigned int adaptive_budget_recovery_threshold = 8u;

float build_aspect_ratio() {
    int fbw = 1280;
    int fbh = 720;
    if (GLFWwindow* ctx = glfwGetCurrentContext())
        glfwGetFramebufferSize(ctx, &fbw, &fbh);

    return fbh == 0 ? 1.f : static_cast<float>(fbw) / static_cast<float>(fbh);
}

GLuint compute_grid_axis(float extent, float cell_size) {
    const float safe_cell_size = glm::max(cell_size, 0.0001f);
    return static_cast<GLuint>(glm::max(1.0f, glm::ceil(extent / safe_cell_size)));
}
}

gpu_fluid_system_component::gpu_fluid_system_component(scene_node* owner,
    compute_shader* compute_shader,
    shader* render_shader,
    Mesh* render_mesh,
    std::vector<fluid_particle> particles,
    const fluid_bounds& bounds,
    const glm::vec3& gravity,
    float particle_size,
    float interaction_radius,
    float particle_radius,
    float separation_strength,
    float near_pressure_strength,
    float velocity_damping,
    float viscosity_strength,
    float rest_density,
    unsigned int solver_substeps,
    unsigned int constraint_iterations)
    : transformable(owner, owner),
    compute_shader_(compute_shader),
    render_shader_(render_shader),
    render_mesh_(render_mesh),
    initial_particles_(std::move(particles)),
    bounds_(bounds),
    gravity_(gravity),
    particle_size_(particle_size),
    interaction_radius_(interaction_radius),
    particle_radius_(particle_radius),
    separation_strength_(separation_strength),
    near_pressure_strength_(near_pressure_strength),
    velocity_damping_(velocity_damping),
    viscosity_strength_(viscosity_strength),
    rest_density_(rest_density),
    solver_substeps_(solver_substeps),
    constraint_iterations_(constraint_iterations),
    runtime_solver_substeps_(std::max(1u, solver_substeps)),
    runtime_constraint_iterations_(std::max(1u, constraint_iterations)),
    particle_count_(initial_particles_.size()) {
    rebuild_grid_metadata();
}

gpu_fluid_system_component::~gpu_fluid_system_component() {
    release_gpu_completion_fence();
}

type_id_t gpu_fluid_system_component::type_id() {
    return ::get_type_id<gpu_fluid_system_component>();
}

type_id_t gpu_fluid_system_component::get_type_id() const {
    return type_id();
}

void gpu_fluid_system_component::rebuild_grid_metadata() {
    cell_size_ = glm::max(interaction_radius_, particle_radius_ * 2.f);
    const glm::vec3 extents = glm::max(bounds_.max - bounds_.min, glm::vec3(cell_size_));
    grid_size_x_ = compute_grid_axis(extents.x, cell_size_);
    grid_size_y_ = compute_grid_axis(extents.y, cell_size_);
    grid_size_z_ = compute_grid_axis(extents.z, cell_size_);
    cell_count_ = static_cast<size_t>(grid_size_x_) * static_cast<size_t>(grid_size_y_) * static_cast<size_t>(grid_size_z_);
}

void gpu_fluid_system_component::rebuild_grid_buffers() {
    if (!compute_shader_ || !compute_shader_->is_vaild())
        return;

    rebuild_grid_metadata();

    compute_shader_->use();
    compute_shader_->add_ssbo(cell_head_binding_, std::vector<int>(cell_count_, -1));
    compute_shader_->add_ssbo(particle_next_binding_, std::vector<int>(particle_count_, -1));
}

void gpu_fluid_system_component::attach_to(scene_node* n_node) {
    transformable::attach_to(n_node);
    if (auto* s_manager = n_node ? n_node->get_scene_manager() : nullptr)
        s_manager->register_in(this);
}

bool gpu_fluid_system_component::detach() {
    if (auto* node = get_node()) {
        if (auto* s_manager = node->get_scene_manager())
            s_manager->register_out(this);
    }

    return transformable::detach();
}

void gpu_fluid_system_component::release_gpu_completion_fence() {
    if (gpu_completion_fence_) {
        glDeleteSync(gpu_completion_fence_);
        gpu_completion_fence_ = 0;
    }
}

void gpu_fluid_system_component::update_adaptive_budget() {
    const unsigned int target_substeps = std::max(1u, solver_substeps_);
    const unsigned int target_iterations = std::max(1u, constraint_iterations_);

    runtime_solver_substeps_ = std::min(runtime_solver_substeps_, target_substeps);
    runtime_constraint_iterations_ = std::min(runtime_constraint_iterations_, target_iterations);

    if (!gpu_completion_fence_)
        return;

    const GLenum status = glClientWaitSync(gpu_completion_fence_, 0, 0);
    release_gpu_completion_fence();

    if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED) {
        ++budget_recovery_frames_;
        if (budget_recovery_frames_ >= adaptive_budget_recovery_threshold) {
            if (runtime_constraint_iterations_ < target_iterations)
                ++runtime_constraint_iterations_;
            else if (runtime_solver_substeps_ < target_substeps)
                ++runtime_solver_substeps_;

            budget_recovery_frames_ = 0;
        }
        return;
    }

    budget_recovery_frames_ = 0;
    if (runtime_constraint_iterations_ > 1u)
        --runtime_constraint_iterations_;
    else if (runtime_solver_substeps_ > 1u)
        --runtime_solver_substeps_;
}

void gpu_fluid_system_component::queue_gpu_completion_fence() {
    release_gpu_completion_fence();
    gpu_completion_fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void gpu_fluid_system_component::set_planetary_surface(const glm::vec3& center, float radius, float shell_thickness, float gravity_strength) {
    planetary_surface_enabled_ = true;
    planetary_center_ = center;
    planetary_radius_ = glm::max(radius, particle_radius_);
    planetary_shell_thickness_ = glm::max(shell_thickness, particle_radius_ * 2.f);
    planetary_gravity_strength_ = glm::max(gravity_strength, 0.0f);
}

void gpu_fluid_system_component::ensure_initialized() {
    if (initialized_ || !compute_shader_ || !compute_shader_->is_vaild())
        return;

    compute_shader_->use();
    compute_shader_->add_ssbo(particle_binding_, initial_particles_);
    rebuild_grid_buffers();
    initialized_ = true;
}

void gpu_fluid_system_component::fixed_update(float dt) {
    ensure_initialized();
    if (!initialized_ || !compute_shader_ || particle_count_ == 0 || cell_count_ == 0)
        return;

    update_adaptive_budget();

    glm::vec3 simulation_gravity = gravity_;
    if (const auto* node = get_node()) {
        const glm::mat4 model = node->get_global_matrix_model();
        glm::vec3 basis_x = glm::vec3(model[0]);
        glm::vec3 basis_y = glm::vec3(model[1]);
        glm::vec3 basis_z = glm::vec3(model[2]);

        if (glm::dot(basis_x, basis_x) > 0.000001f
            && glm::dot(basis_y, basis_y) > 0.000001f
            && glm::dot(basis_z, basis_z) > 0.000001f) {
            const glm::mat3 world_from_local(
                glm::normalize(basis_x),
                glm::normalize(basis_y),
                glm::normalize(basis_z));
            simulation_gravity = glm::transpose(world_from_local) * gravity_;
        }
    }

    const GLuint particle_groups_x = static_cast<GLuint>((particle_count_ + 63u) / 64u);
    const GLuint cell_groups_x = static_cast<GLuint>((cell_count_ + 63u) / 64u);
    const unsigned int substeps = std::max(1u, runtime_solver_substeps_);
    const unsigned int constraint_iterations = std::max(1u, runtime_constraint_iterations_);
    const float substep_dt = dt / static_cast<float>(substeps);

    compute_shader_->use();
    compute_shader_->set_uni_vec3("gravity", simulation_gravity);
    compute_shader_->set_uni_vec3("boundsMin", bounds_.min);
    compute_shader_->set_uni_vec3("boundsMax", bounds_.max);
    compute_shader_->set_uni_float("restitution", bounds_.restitution);
    compute_shader_->set_uni_float("collisionDamping", bounds_.damping);
    compute_shader_->set_uni_float("interactionRadius", interaction_radius_);
    compute_shader_->set_uni_float("particleRadius", particle_radius_);
    compute_shader_->set_uni_float("separationStrength", separation_strength_);
    compute_shader_->set_uni_float("nearPressureStrength", near_pressure_strength_);
    compute_shader_->set_uni_float("velocityDamping", velocity_damping_);
    compute_shader_->set_uni_float("viscosityStrength", viscosity_strength_);
    compute_shader_->set_uni_float("restDensity", rest_density_);
    compute_shader_->set_uni_float("cellSize", cell_size_);
    compute_shader_->set_uni_int("gridSizeX", static_cast<int>(grid_size_x_));
    compute_shader_->set_uni_int("gridSizeY", static_cast<int>(grid_size_y_));
    compute_shader_->set_uni_int("gridSizeZ", static_cast<int>(grid_size_z_));
    compute_shader_->set_uni_int("simulationMode", planetary_surface_enabled_ ? 1 : 0);
    compute_shader_->set_uni_vec3("planetaryCenter", planetary_center_);
    compute_shader_->set_uni_float("planetaryRadius", planetary_radius_);
    compute_shader_->set_uni_float("planetaryShellThickness", planetary_shell_thickness_);
    compute_shader_->set_uni_float("planetaryGravityStrength", planetary_gravity_strength_);

    for (unsigned int substep = 0; substep < substeps; ++substep) {
        compute_shader_->set_uni_float("dt", substep_dt);

        compute_shader_->set_uni_int("passMode", 0);
        compute_shader_->dispatch({ particle_groups_x, 1u, 1u });

        for (unsigned int iteration = 0; iteration < constraint_iterations; ++iteration) {
            compute_shader_->set_uni_int("passMode", 1);
            compute_shader_->dispatch({ cell_groups_x, 1u, 1u });

            compute_shader_->set_uni_int("passMode", 2);
            compute_shader_->dispatch({ particle_groups_x, 1u, 1u });

            compute_shader_->set_uni_int("passMode", 3);
            compute_shader_->dispatch({ particle_groups_x, 1u, 1u });

            compute_shader_->set_uni_int("passMode", 4);
            compute_shader_->dispatch({ particle_groups_x, 1u, 1u });

            compute_shader_->set_uni_int("passMode", 5);
            compute_shader_->dispatch({ particle_groups_x, 1u, 1u });
        }

        compute_shader_->set_uni_int("passMode", 1);
        compute_shader_->dispatch({ cell_groups_x, 1u, 1u });

        compute_shader_->set_uni_int("passMode", 2);
        compute_shader_->dispatch({ particle_groups_x, 1u, 1u });

        compute_shader_->set_uni_int("passMode", 6);
        compute_shader_->dispatch({ particle_groups_x, 1u, 1u });
    }

    queue_gpu_completion_fence();
}

void gpu_fluid_system_component::draw(Camera* camera) const {
    if (!camera || !render_shader_ || !render_mesh_ || !compute_shader_ || particle_count_ == 0)
        return;

    const GLuint ssbo = compute_shader_->get_ssbo_id(particle_binding_);
    if (ssbo == 0)
        return;

    render_shader_->use();
    render_shader_->set_uniform_mat4("systemModel", get_node()->get_global_matrix_model());
    render_shader_->set_uniform_mat4("view", camera->GetViewMatrix());
    render_shader_->set_uniform_mat4("projection", camera->GetProjectionMatrix(build_aspect_ratio()));
    render_shader_->set_uni_float("particleSize", particle_size_);
    render_shader_->set_uni_vec3("particleColor", glm::vec3(0.18f, 0.58f, 1.0f));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, ssbo);
    render_mesh_->DrawInstanced(static_cast<GLsizei>(particle_count_));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

GLuint gpu_fluid_system_component::get_ssbo_id() const {
    return compute_shader_ ? compute_shader_->get_ssbo_id(particle_binding_) : 0;
}

void gpu_fluid_system_component::set_bounds(const fluid_bounds& bounds) {
    bounds_ = bounds;
    rebuild_grid_metadata();
    if (initialized_)
        rebuild_grid_buffers();
}
