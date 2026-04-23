#include "gpu_particle_system_component.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Camera.h"
#include "Mesh.h"
#include "Shader.h"

namespace {
    float build_aspect_ratio() {
        int fbw = 1280;
        int fbh = 720;
        if (GLFWwindow* ctx = glfwGetCurrentContext())
            glfwGetFramebufferSize(ctx, &fbw, &fbh);

        return fbh == 0 ? 1.f : static_cast<float>(fbw) / static_cast<float>(fbh);
    }
}

gpu_particle_system_component::gpu_particle_system_component(scene_node* owner,
    compute_shader* compute_shader,
    shader* render_shader,
    Mesh* render_mesh,
    unit_system* unit_system,
    std::vector<physics_data> particles,
    float particle_size,
    float simulation_speed)
    : transformable(owner, owner),
    compute_shader_(compute_shader),
    render_shader_(render_shader),
    render_mesh_(render_mesh),
    unit_system_(unit_system),
    initial_particles_(std::move(particles)),
    particle_size_(particle_size),
    simulation_speed_(simulation_speed),
    particle_count_(initial_particles_.size()) {
}

type_id_t gpu_particle_system_component::type_id() {
    return ::get_type_id<gpu_particle_system_component>();
}

type_id_t gpu_particle_system_component::get_type_id() const {
    return type_id();
}

void gpu_particle_system_component::attach_to(scene_node* n_node) {
    transformable::attach_to(n_node);
    if (auto* s_manager = n_node ? n_node->get_scene_manager() : nullptr)
        s_manager->register_in(this);
}

bool gpu_particle_system_component::detach() {
    if (auto* node = get_node()) {
        if (auto* s_manager = node->get_scene_manager())
            s_manager->register_out(this);
    }

    return transformable::detach();
}

void gpu_particle_system_component::ensure_initialized() {
    if (initialized_ || !compute_shader_ || !compute_shader_->is_vaild())
        return;

    compute_shader_->use();
    compute_shader_->add_ssbo(physics_binding_, initial_particles_);
    initialized_ = true;
}

void gpu_particle_system_component::fixed_update(float dt) {
    ensure_initialized();
    if (!initialized_ || !compute_shader_ || !unit_system_ || particle_count_ == 0)
        return;

    simulation_time_ += dt;
    const GLuint groups_x = static_cast<GLuint>((particle_count_ + 63u) / 64u);

    compute_shader_->use();
    compute_shader_->set_uni_float("G", unit_system_->scaled_G());
    compute_shader_->set_uni_float("dt", unit_system_->time(dt) * simulation_speed_);
    compute_shader_->set_uni_float("rawDt", dt);
    compute_shader_->set_uni_float("simulationTime", simulation_time_);
    compute_shader_->dispatch({ groups_x, 1u, 1u });
}

void gpu_particle_system_component::draw(Camera* camera) const {
    if (!camera || !render_shader_ || !render_mesh_ || !compute_shader_ || particle_count_ == 0)
        return;

    const GLuint ssbo = compute_shader_->get_ssbo_id(physics_binding_);
    if (ssbo == 0)
        return;

    render_shader_->use();
    render_shader_->set_uniform_mat4("systemModel", get_node()->get_global_matrix_model());
    render_shader_->set_uniform_mat4("view", camera->GetViewMatrix());
    render_shader_->set_uniform_mat4("projection", camera->GetProjectionMatrix(build_aspect_ratio()));
    render_shader_->set_uni_float("particleSize", particle_size_);
    render_shader_->set_uni_vec3("particleColor", particle_color_);
    render_shader_->set_uni_float("particleAlpha", particle_alpha_);
    render_shader_->set_uni_float("particleGlowStrength", particle_glow_strength_);
    render_shader_->set_uni_float("particleSizeJitter", particle_size_jitter_);
    render_shader_->set_uni_int("particleVisualMode", particle_visual_mode_);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(depth_write_enabled_ ? GL_TRUE : GL_FALSE);
    if (additive_blend_enabled_ || particle_alpha_ < 0.999f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, additive_blend_enabled_ ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    }
    else {
        glDisable(GL_BLEND);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, physics_binding_, ssbo);
    render_mesh_->DrawInstanced(static_cast<GLsizei>(particle_count_));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, physics_binding_, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

GLuint gpu_particle_system_component::get_ssbo_id() const {
    return compute_shader_ ? compute_shader_->get_ssbo_id(physics_binding_) : 0;
}
