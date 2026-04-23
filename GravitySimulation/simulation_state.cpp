#include "simulation_state.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/quaternion.hpp>

#include "cloth_scene.h"
#include "engine.h"
#include "fluid_scene.h"
#include "galactic_scene.h"
#include "galactic_stress_scene.h"
#include "g_shape.h"
#include "gpu_fluid_system_component.h"
#include "gpu_particle_system_component.h"
#include "input_system.h"
#include "ray_cast.h"
#include "spatial_query.h"

namespace {
struct focus_pick_result {
    renderer* render = nullptr;
    glm::vec3 world_position = glm::vec3(0.f);
    float world_radius = 1.f;
};

float ease_out_cubic(float t) {
    const float inv = 1.f - glm::clamp(t, 0.f, 1.f);
    return 1.f - inv * inv * inv;
}

float ease_in_out_cubic(float t) {
    const float clamped = glm::clamp(t, 0.f, 1.f);
    if (clamped < 0.5f)
        return 4.f * clamped * clamped * clamped;

    const float f = -2.f * clamped + 2.f;
    return 1.f - (f * f * f) / 2.f;
}

float damp_factor(float dt, float sharpness) {
    return 1.f - std::exp(-glm::max(dt, 0.f) * sharpness);
}

std::unique_ptr<scene> create_example_scene(simulation_state::example_scene_kind scene_kind, sim::time* time) {
    switch (scene_kind) {
    case simulation_state::example_scene_kind::fluid:
        return std::make_unique<fluid_scene>(time);
    case simulation_state::example_scene_kind::cloth:
        return std::make_unique<cloth_scene>(time);
    case simulation_state::example_scene_kind::galactic:
        return std::make_unique<galactic_scene>(time);
    case simulation_state::example_scene_kind::galactic_stress:
        return std::make_unique<galactic_stress_scene>(time);
    }

    return std::make_unique<fluid_scene>(time);
}

const char* get_scene_name(simulation_state::example_scene_kind scene_kind) {
    switch (scene_kind) {
    case simulation_state::example_scene_kind::fluid:
        return "Fluid";
    case simulation_state::example_scene_kind::cloth:
        return "Cloth";
    case simulation_state::example_scene_kind::galactic:
        return "Galactic";
    case simulation_state::example_scene_kind::galactic_stress:
        return "Galactic Stress";
    }

    return "Fluid";
}

std::string build_window_title(simulation_state::example_scene_kind scene_kind) {
    return std::string("GravitySimulation - ") + get_scene_name(scene_kind)
        + " [F1 Fluid | F2 Cloth | F3 Galactic | F4 Stress]";
}

bool poll_scene_switch_key(int glfw_key, bool& previous_down) {
    const bool is_down = input_system::is_key_down(glfw_key);
    const bool pressed = is_down && !previous_down;
    previous_down = is_down;
    return pressed;
}

bool poll_toggle_key(int glfw_key, bool& previous_down) {
    const bool is_down = input_system::is_key_down(glfw_key);
    const bool pressed = is_down && !previous_down;
    previous_down = is_down;
    return pressed;
}

fluid_debug_visualization_mode next_fluid_debug_mode(fluid_debug_visualization_mode mode) {
    constexpr int mode_count = 10;
    int value = (static_cast<int>(mode) + 1) % mode_count;
    return static_cast<fluid_debug_visualization_mode>(value);
}

fluid_debug_visualization_mode previous_fluid_debug_mode(fluid_debug_visualization_mode mode) {
    constexpr int mode_count = 10;
    int value = (static_cast<int>(mode) + mode_count - 1) % mode_count;
    return static_cast<fluid_debug_visualization_mode>(value);
}

const char* get_fluid_debug_mode_name(fluid_debug_visualization_mode mode) {
    switch (mode) {
    case fluid_debug_visualization_mode::none:
        return "none";
    case fluid_debug_visualization_mode::ocean_fill:
        return "ocean_fill";
    case fluid_debug_visualization_mode::constraint:
        return "constraint";
    case fluid_debug_visualization_mode::lambda:
        return "lambda";
    case fluid_debug_visualization_mode::flow_direction:
        return "flow_direction";
    case fluid_debug_visualization_mode::distance_to_floor:
        return "distance_to_floor";
    case fluid_debug_visualization_mode::distance_to_water_surface:
        return "distance_to_water_surface";
    case fluid_debug_visualization_mode::coriolis_strength:
        return "coriolis_strength";
    case fluid_debug_visualization_mode::tidal_strength:
        return "tidal_strength";
    case fluid_debug_visualization_mode::combined_flow:
        return "combined_flow";
    }

    return "unknown";
}

MeshData create_fullscreen_quad_mesh() {
    MeshData data;
    Vertex v0{};
    v0.Position = glm::vec3(-1.f, -1.f, 0.f);
    v0.Normal = glm::vec3(0.f, 0.f, 1.f);
    Vertex v1{};
    v1.Position = glm::vec3(1.f, -1.f, 0.f);
    v1.Normal = glm::vec3(0.f, 0.f, 1.f);
    Vertex v2{};
    v2.Position = glm::vec3(1.f, 1.f, 0.f);
    v2.Normal = glm::vec3(0.f, 0.f, 1.f);
    Vertex v3{};
    v3.Position = glm::vec3(-1.f, 1.f, 0.f);
    v3.Normal = glm::vec3(0.f, 0.f, 1.f);
    data.vertecies = { v0, v1, v2, v3 };
    data.indices = { 0u, 1u, 2u, 0u, 2u, 3u };
    return data;
}

MeshData create_bounding_box_line_mesh() {
    MeshData data;
    const glm::vec3 corners[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f}
    };

    for (const auto& corner : corners) {
        Vertex vertex{};
        vertex.Position = corner;
        vertex.Normal = glm::vec3(0.f, 1.f, 0.f);
        data.vertecies.push_back(vertex);
    }

    data.indices = {
        0u, 1u, 1u, 3u, 3u, 2u, 2u, 0u,
        4u, 5u, 5u, 7u, 7u, 6u, 6u, 4u,
        0u, 4u, 1u, 5u, 2u, 6u, 3u, 7u
    };
    return data;
}

float estimate_renderer_radius(const renderer& render) {
    const glm::mat4 model = render.get_visual_model_matrix_without_translation();
    return std::max({
        glm::length(glm::vec3(model[0])),
        glm::length(glm::vec3(model[1])),
        glm::length(glm::vec3(model[2])),
        1.0f
    });
}

void orient_camera_towards(Camera& camera, const glm::vec3& target_position) {
    const glm::vec3 camera_position = camera.get_node()->get_global_position();
    const glm::vec3 delta = target_position - camera_position;
    if (glm::dot(delta, delta) <= 0.0001f)
        return;

    const glm::vec3 direction = glm::normalize(delta);
    const float yaw = glm::degrees(std::atan2(-direction.x, -direction.z));
    const float pitch = glm::degrees(std::asin(glm::clamp(direction.y, -1.f, 1.f)));
    const glm::quat q_pitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    const glm::quat q_yaw = glm::angleAxis(glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    const glm::vec3 euler = glm::degrees(glm::eulerAngles(q_yaw * q_pitch));

    camera.Yaw = yaw;
    camera.Pitch = pitch;
    camera.get_node()->set_global_rotation(euler);
}

std::optional<focus_pick_result> pick_renderer_from_mouse(const scene& scene_context, Camera& camera) {
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return std::nullopt;

    int window_width = 0;
    int window_height = 0;
    glfwGetWindowSize(window, &window_width, &window_height);
    if (window_width <= 0 || window_height <= 0)
        return std::nullopt;

    const glm::vec3 mouse_pos = input_system::get_mouse_pos();
    const float aspect = static_cast<float>(window_width) / static_cast<float>(window_height);
    const glm::mat4 view = camera.GetViewMatrix();
    const glm::mat4 projection = camera.GetProjectionMatrix(aspect);
    const glm::mat4 inverse_view_projection = glm::inverse(projection * view);
    const world_ray pick_ray = make_world_ray_from_screen(glm::vec2(mouse_pos.x, mouse_pos.y), glm::ivec2(window_width, window_height), inverse_view_projection);

    ray_cast picker(pick_ray);
    if (!picker.cast(scene_context))
        return std::nullopt;

    const ray_cast_hit* closest_hit = picker.get_closest_hit();
    if (!closest_hit || !closest_hit->render)
        return std::nullopt;

    return focus_pick_result{
        closest_hit->render,
        closest_hit->bounds.get_center(),
        estimate_renderer_radius(*closest_hit->render)
    };
}

float compute_particle_surface_detail_blend(const scene& scene_context, Camera& camera) {
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return 0.0f;

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (framebuffer_width <= 0 || framebuffer_height <= 0)
        return 0.0f;

    const float aspect = static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height);
    const glm::mat4 view = camera.GetViewMatrix();
    const glm::mat4 projection = camera.GetProjectionMatrix(aspect);
    float max_projected_radius = 0.0f;

    for (const auto* system : scene_context.get_gpu_fluid_systems()) {
        if (!system || !system->supports_particle_surface_pass() || !system->get_node())
            continue;

        const glm::mat4 model = system->get_node()->get_global_matrix_model();
        const glm::vec3 world_center = glm::vec3(model * glm::vec4(system->get_planetary_center(), 1.0f));
        const glm::vec4 center_view = view * glm::vec4(world_center, 1.0f);
        const float view_depth = glm::max(-center_view.z, 0.001f);
        const float system_scale = std::max(
            std::max(glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1]))),
            std::max(glm::length(glm::vec3(model[2])), 1.0f));
        const float world_radius = system->get_planetary_water_surface_radius() * system_scale;
        const float projected_radius = projection[1][1] * world_radius * static_cast<float>(framebuffer_height) / view_depth;
        max_projected_radius = glm::max(max_projected_radius, projected_radius);
    }

    return glm::clamp((max_projected_radius - 120.0f) / 300.0f, 0.0f, 1.0f);
}

constexpr int planetary_water_atlas_default_width = 2048;
constexpr int planetary_water_atlas_default_height = 1024;
}

simulation_state::simulation_state(std::unique_ptr<scene> scene)
    : scene_(std::move(scene)) {
}

simulation_state::simulation_state(example_scene_kind scene_kind)
    : scene_kind_(scene_kind) {
}

void simulation_state::ensure_scene_depth_texture(int width, int height) {
    if (width <= 0 || height <= 0)
        return;

    if (scene_depth_texture_ == 0)
        glGenTextures(1, &scene_depth_texture_);

    glBindTexture(GL_TEXTURE_2D, scene_depth_texture_);
    if (scene_depth_texture_width_ != width || scene_depth_texture_height_ != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        scene_depth_texture_width_ = width;
        scene_depth_texture_height_ = height;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void simulation_state::capture_scene_depth_texture(int width, int height) {
    ensure_scene_depth_texture(width, height);
    if (scene_depth_texture_ == 0 || width <= 0 || height <= 0)
        return;

    glBindTexture(GL_TEXTURE_2D, scene_depth_texture_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void simulation_state::release_scene_depth_texture() {
    if (scene_depth_texture_ != 0) {
        glDeleteTextures(1, &scene_depth_texture_);
        scene_depth_texture_ = 0;
    }

    scene_depth_texture_width_ = 0;
    scene_depth_texture_height_ = 0;
}

void simulation_state::initialize_particle_surface_composite_resources() {
    if (!scene_)
        return;

    auto& assets = scene_->get_asset_manager();
    if (!particle_surface_composite_shader_) {
        particle_surface_composite_shader_ = assets.create_shader(
            "particle.surface.composite",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/particle_surface_composite.fs.shader");
    }
    if (!particle_surface_blur_shader_) {
        particle_surface_blur_shader_ = assets.create_shader(
            "particle.surface.blur",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/particle_surface_blur.fs.shader");
    }
    if (!planetary_water_atlas_shader_) {
        planetary_water_atlas_shader_ = assets.create_shader(
            "planetary.water.atlas.input",
            "GravitySimulation/planetary_water_atlas_input.vs.shader",
            "GravitySimulation/planetary_water_atlas_input.fs.shader");
    }
    if (!planetary_water_atlas_blur_shader_) {
        planetary_water_atlas_blur_shader_ = assets.create_shader(
            "planetary.water.atlas.blur",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/planetary_water_atlas_blur.fs.shader");
    }
    if (!planetary_water_atlas_temporal_shader_) {
        planetary_water_atlas_temporal_shader_ = assets.create_shader(
            "planetary.water.atlas.temporal",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/planetary_water_atlas_temporal.fs.shader");
    }
    if (!planetary_water_shell_shader_) {
        planetary_water_shell_shader_ = assets.create_shader(
            "planetary.water.shell",
            "GravitySimulation/planetary_water_shell.vs.shader",
            "GravitySimulation/planetary_water_shell.fs.shader");
    }
    static MeshData fullscreen_quad_mesh_data = create_fullscreen_quad_mesh();
    if (!particle_surface_composite_mesh_)
        particle_surface_composite_mesh_ = assets.create_mesh(fullscreen_quad_mesh_data);
    static MeshData planetary_water_shell_mesh_data = g_shape::generate_sphere();
    if (!planetary_water_shell_mesh_)
        planetary_water_shell_mesh_ = assets.create_mesh(planetary_water_shell_mesh_data);
}

void simulation_state::ensure_planetary_water_atlas_targets(int width, int height) {
    if (width <= 0 || height <= 0)
        return;

    if (planetary_water_atlas_framebuffer_ == 0)
        glGenFramebuffers(1, &planetary_water_atlas_framebuffer_);
    if (planetary_water_atlas_texture_ == 0)
        glGenTextures(1, &planetary_water_atlas_texture_);
    if (planetary_water_atlas_ping_texture_ == 0)
        glGenTextures(1, &planetary_water_atlas_ping_texture_);
    if (planetary_water_atlas_history_texture_ == 0)
        glGenTextures(1, &planetary_water_atlas_history_texture_);

    if (planetary_water_atlas_framebuffer_ == 0 || planetary_water_atlas_texture_ == 0 || planetary_water_atlas_ping_texture_ == 0 || planetary_water_atlas_history_texture_ == 0)
        return;

    if (planetary_water_atlas_width_ == width && planetary_water_atlas_height_ == height)
        return;

    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_ping_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_history_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const float clear_history[4] = { 0.f, 0.f, 0.f, 0.f };
    glClearTexImage(planetary_water_atlas_history_texture_, 0, GL_RGBA, GL_FLOAT, clear_history);

    glBindFramebuffer(GL_FRAMEBUFFER, planetary_water_atlas_framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, planetary_water_atlas_texture_, 0);
    const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, draw_buffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    planetary_water_atlas_width_ = width;
    planetary_water_atlas_height_ = height;
}

void simulation_state::release_planetary_water_atlas_resources() {
    if (planetary_water_atlas_framebuffer_ != 0) {
        glDeleteFramebuffers(1, &planetary_water_atlas_framebuffer_);
        planetary_water_atlas_framebuffer_ = 0;
    }

    if (planetary_water_atlas_texture_ != 0) {
        glDeleteTextures(1, &planetary_water_atlas_texture_);
        planetary_water_atlas_texture_ = 0;
    }
    if (planetary_water_atlas_ping_texture_ != 0) {
        glDeleteTextures(1, &planetary_water_atlas_ping_texture_);
        planetary_water_atlas_ping_texture_ = 0;
    }
    if (planetary_water_atlas_history_texture_ != 0) {
        glDeleteTextures(1, &planetary_water_atlas_history_texture_);
        planetary_water_atlas_history_texture_ = 0;
    }

    planetary_water_atlas_width_ = 0;
    planetary_water_atlas_height_ = 0;
}

void simulation_state::blur_planetary_water_atlas() {
    if (!planetary_water_atlas_blur_shader_ || !particle_surface_composite_mesh_ || planetary_water_atlas_framebuffer_ == 0 || planetary_water_atlas_texture_ == 0 || planetary_water_atlas_ping_texture_ == 0)
        return;

    GLint previous_framebuffer = 0;
    GLint previous_viewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, planetary_water_atlas_framebuffer_);
    glViewport(0, 0, planetary_water_atlas_width_, planetary_water_atlas_height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    planetary_water_atlas_blur_shader_->use();
    planetary_water_atlas_blur_shader_->set_uni_int("inputTexture", 0);
    planetary_water_atlas_blur_shader_->set_uni_float("blurRadiusScale", 1.18f);

    const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, planetary_water_atlas_ping_texture_, 0);
    glDrawBuffers(1, draw_buffers);
    planetary_water_atlas_blur_shader_->set_uni_vec2("blurDirection", glm::vec2(1.0f, 0.0f));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_texture_);
    particle_surface_composite_mesh_->Draw();

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, planetary_water_atlas_texture_, 0);
    glDrawBuffers(1, draw_buffers);
    planetary_water_atlas_blur_shader_->set_uni_vec2("blurDirection", glm::vec2(0.0f, 1.0f));
    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_ping_texture_);
    particle_surface_composite_mesh_->Draw();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void simulation_state::stabilize_planetary_water_atlas() {
    if (!planetary_water_atlas_temporal_shader_ || !particle_surface_composite_mesh_ || planetary_water_atlas_framebuffer_ == 0 || planetary_water_atlas_texture_ == 0 || planetary_water_atlas_ping_texture_ == 0 || planetary_water_atlas_history_texture_ == 0)
        return;

    GLint previous_framebuffer = 0;
    GLint previous_viewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, planetary_water_atlas_framebuffer_);
    glViewport(0, 0, planetary_water_atlas_width_, planetary_water_atlas_height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, planetary_water_atlas_ping_texture_, 0);
    glDrawBuffers(1, draw_buffers);

    planetary_water_atlas_temporal_shader_->use();
    planetary_water_atlas_temporal_shader_->set_uni_int("currentAtlasTexture", 0);
    planetary_water_atlas_temporal_shader_->set_uni_int("historyAtlasTexture", 1);
    planetary_water_atlas_temporal_shader_->set_uni_float("historyBlend", 0.90f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_texture_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_history_texture_);
    glActiveTexture(GL_TEXTURE0);
    particle_surface_composite_mesh_->Draw();

    glCopyImageSubData(planetary_water_atlas_ping_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
        planetary_water_atlas_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
        planetary_water_atlas_width_, planetary_water_atlas_height_, 1);
    glCopyImageSubData(planetary_water_atlas_ping_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
        planetary_water_atlas_history_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
        planetary_water_atlas_width_, planetary_water_atlas_height_, 1);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void simulation_state::blur_particle_surface_targets() {
    if (!particle_surface_blur_shader_ || !particle_surface_composite_mesh_)
        return;

    const auto& targets = render_pipeline_.get_particle_surface_targets();
    if (targets.blur_framebuffer == 0 || targets.width <= 0 || targets.height <= 0)
        return;

    GLint previous_framebuffer = 0;
    GLint previous_viewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, targets.blur_framebuffer);
    glViewport(0, 0, targets.width, targets.height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    particle_surface_blur_shader_->use();
    particle_surface_blur_shader_->set_uni_int("inputTexture", 0);
    const float detail_blend = (scene_ && cam_) ? compute_particle_surface_detail_blend(*scene_, *cam_) : 0.0f;
    const int coverage_pass_count = 3;
    const int front_depth_pass_count = 2;

    const auto blur_texture = [&](GLuint source_texture, GLuint ping_texture, GLuint destination_texture, int blur_mode, int pass_count, float radius_scale) {
        const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };

        GLuint current_source = source_texture;
        GLuint current_destination = destination_texture;
        for (int pass_index = 0; pass_index < pass_count; ++pass_index) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ping_texture, 0);
            glDrawBuffers(1, draw_buffers);
            particle_surface_blur_shader_->set_uni_int("blurMode", blur_mode);
            particle_surface_blur_shader_->set_uni_float("blurRadiusScale", radius_scale);
            particle_surface_blur_shader_->set_uni_vec2("blurDirection", glm::vec2(1.0f, 0.0f));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, current_source);
            particle_surface_composite_mesh_->Draw();

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, current_destination, 0);
            glDrawBuffers(1, draw_buffers);
            particle_surface_blur_shader_->set_uni_vec2("blurDirection", glm::vec2(0.0f, 1.0f));
            glBindTexture(GL_TEXTURE_2D, ping_texture);
            particle_surface_composite_mesh_->Draw();

            current_source = current_destination;
            current_destination = current_destination == destination_texture ? ping_texture : destination_texture;
        }

        if (current_source != destination_texture) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destination_texture, 0);
            glDrawBuffers(1, draw_buffers);
            particle_surface_blur_shader_->set_uni_float("blurRadiusScale", 1.0f);
            particle_surface_blur_shader_->set_uni_vec2("blurDirection", glm::vec2(0.0f, 0.0f));
            glBindTexture(GL_TEXTURE_2D, current_source);
            particle_surface_composite_mesh_->Draw();
        }
    };

    const float coverage_radius_scale = glm::mix(1.00f, 1.45f, detail_blend);
    const float front_depth_radius_scale = glm::mix(0.94f, 1.18f, detail_blend);
    blur_texture(targets.coverage_texture, targets.coverage_ping_texture, targets.coverage_blur_texture, 0, coverage_pass_count, coverage_radius_scale);
    blur_texture(targets.front_depth_texture, targets.front_depth_ping_texture, targets.front_depth_blur_texture, 1, front_depth_pass_count, front_depth_radius_scale);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void simulation_state::render_planetary_water_atlas_input(const gpu_fluid_system_component& system) {
    if (!planetary_water_atlas_shader_ || planetary_water_atlas_framebuffer_ == 0 || planetary_water_atlas_texture_ == 0)
        return;

    GLint previous_framebuffer = 0;
    GLint previous_viewport[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, planetary_water_atlas_framebuffer_);
    glViewport(0, 0, planetary_water_atlas_width_, planetary_water_atlas_height_);
    const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
    glClearBufferfv(GL_COLOR, 0, clear_color);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    glDepthMask(GL_FALSE);

    system.draw_planetary_water_atlas_input(
        planetary_water_atlas_shader_,
        glm::ivec2(planetary_water_atlas_width_, planetary_water_atlas_height_));

    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);

    blur_planetary_water_atlas();
    stabilize_planetary_water_atlas();
}

void simulation_state::render_planetary_water_shell(const gpu_fluid_system_component& system, const glm::vec3& light_position, const glm::vec3& light_color, float light_intensity) {
    if (!planetary_water_shell_shader_ || !planetary_water_shell_mesh_ || planetary_water_atlas_texture_ == 0 || !cam_)
        return;

    const auto* node = system.get_node();
    if (!node)
        return;

    const auto& terrain_profile = system.get_planetary_terrain_profile();

    planetary_water_shell_shader_->use();
    planetary_water_shell_shader_->set_uniform_mat4("systemModel", node->get_global_matrix_model());
    planetary_water_shell_shader_->set_uniform_mat4("view", cam_->GetViewMatrix());
    int fbw = 1280;
    int fbh = 720;
    if (GLFWwindow* ctx = glfwGetCurrentContext())
        glfwGetFramebufferSize(ctx, &fbw, &fbh);
    const float aspect = fbh == 0 ? 1.0f : static_cast<float>(fbw) / static_cast<float>(fbh);
    planetary_water_shell_shader_->set_uniform_mat4("projection", cam_->GetProjectionMatrix(aspect));
    planetary_water_shell_shader_->set_uni_vec3("viewPos", cam_->get_transform()->get_global_position());
    planetary_water_shell_shader_->set_uni_vec3("lightPos", light_position);
    planetary_water_shell_shader_->set_uni_vec3("lightColor", light_color);
    planetary_water_shell_shader_->set_uni_float("intensity", light_intensity);
    planetary_water_shell_shader_->set_uni_float("time", static_cast<float>(glfwGetTime()));
    planetary_water_shell_shader_->set_uni_vec3("planetaryCenter", system.get_planetary_center());
    planetary_water_shell_shader_->set_uni_float("planetaryRadius", system.get_planetary_radius());
    planetary_water_shell_shader_->set_uni_float("planetaryShellThickness", system.get_planetary_shell_thickness());
    planetary_water_shell_shader_->set_uni_float("planetaryWaterSurfaceRadius", system.get_planetary_water_surface_radius());
    planetary_water_shell_shader_->set_uni_int("planetaryTerrainEnabled", system.is_planetary_terrain_enabled() ? 1 : 0);
    planetary_water_shell_shader_->set_uni_float("terrainSeaLevel", terrain_profile.sea_level);
    planetary_water_shell_shader_->set_uni_float("terrainContinentFrequency", terrain_profile.continent_frequency);
    planetary_water_shell_shader_->set_uni_float("terrainContinentWarpStrength", terrain_profile.continent_warp_strength);
    planetary_water_shell_shader_->set_uni_float("terrainLargeFrequency", terrain_profile.large_frequency);
    planetary_water_shell_shader_->set_uni_float("terrainMediumFrequency", terrain_profile.medium_frequency);
    planetary_water_shell_shader_->set_uni_float("terrainDetailFrequency", terrain_profile.detail_frequency);
    planetary_water_shell_shader_->set_uni_float("terrainRidgeFrequency", terrain_profile.ridge_frequency);
    planetary_water_shell_shader_->set_uni_float("terrainCraterStrength", terrain_profile.crater_strength);
    planetary_water_shell_shader_->set_uni_float("terrainMountainSharpness", terrain_profile.mountain_sharpness);
    planetary_water_shell_shader_->set_uni_float("terrainReliefStrength", terrain_profile.relief_strength);
    planetary_water_shell_shader_->set_uni_float("terrainDisplacementStrength", terrain_profile.displacement_strength);
    planetary_water_shell_shader_->set_uni_float("terrainContinentContrast", terrain_profile.continent_contrast);
    planetary_water_shell_shader_->set_uni_float("terrainEarthMacroContinentStrength", terrain_profile.earth_macro_continent_strength);
    planetary_water_shell_shader_->set_uni_float("terrainArchipelagoStrength", terrain_profile.archipelago_strength);
    const glm::mat4 system_model = node->get_global_matrix_model();
    const float system_scale = std::max(
        std::max(glm::length(glm::vec3(system_model[0])), glm::length(glm::vec3(system_model[1]))),
        std::max(glm::length(glm::vec3(system_model[2])), 1.0f));
    planetary_water_shell_shader_->set_uni_vec3("planetaryCenterWorld", glm::vec3(system_model * glm::vec4(system.get_planetary_center(), 1.0f)));
    planetary_water_shell_shader_->set_uni_float("planetarySolidRadiusWorld", system.get_planetary_radius() * system_scale);
    planetary_water_shell_shader_->set_uni_float("planetDepthBiasWorld", std::max(system.get_planetary_shell_thickness() * system_scale * 0.42f, system.get_planetary_radius() * system_scale * 0.004f));
    planetary_water_shell_shader_->set_uni_int("waterAtlasTexture", 0);
    planetary_water_shell_shader_->set_uni_int("waterLevelTextureAvailable", system.get_planetary_water_level_texture() != 0 ? 1 : 0);
    planetary_water_shell_shader_->set_uni_int("waterLevelTexture", 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planetary_water_atlas_texture_);
    if (system.get_planetary_water_level_texture() != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, system.get_planetary_water_level_texture());
    }
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    planetary_water_shell_mesh_->Draw();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    if (system.get_planetary_water_level_texture() != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void simulation_state::render_particle_surface_composite() {
    if (!particle_surface_composite_shader_ || !particle_surface_composite_mesh_ || !cam_)
        return;

    const auto& targets = render_pipeline_.get_particle_surface_targets();
    if (targets.coverage_blur_texture == 0 || (targets.front_depth_blur_texture == 0 && targets.front_depth_texture == 0))
        return;

    const GLuint surface_depth_texture = targets.front_depth_blur_texture != 0
        ? targets.front_depth_blur_texture
        : targets.front_depth_texture;

    particle_surface_composite_shader_->use();
    particle_surface_composite_shader_->set_uni_int("particleSurfaceCoverageTexture", 0);
    particle_surface_composite_shader_->set_uni_int("particleSurfaceDepthTexture", 1);
    particle_surface_composite_shader_->set_uni_int("sceneDepthTexture", 2);
    particle_surface_composite_shader_->set_uni_int("useSceneDepth", scene_depth_texture_ != 0 ? 1 : 0);
    particle_surface_composite_shader_->set_uni_float("particleSurfaceDetailBlend", scene_ ? compute_particle_surface_detail_blend(*scene_, *cam_) : 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, targets.coverage_blur_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, surface_depth_texture);
    if (scene_depth_texture_ != 0) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, scene_depth_texture_);
    }
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    particle_surface_composite_mesh_->Draw();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void simulation_state::on_enter(engine& engine) {
    if (!scene_)
        scene_ = create_example_scene(scene_kind_, &engine.get_time());

    loading_feedback_presenter_ = create_loading_feedback_presenter(engine);
    loading_feedback_active_ = false;
    scene_->init();
    initialize_bounding_box_debug_resources();
    initialize_particle_surface_composite_resources();
    cam_ = scene_->get_main_camera();
    update_loading_feedback(engine);

    for (auto* system : scene_->get_gpu_fluid_systems()) {
        if (system)
            system->set_debug_visualization_mode(fluid_debug_mode_);
    }

    if (GLFWwindow* window = engine.get_window())
        glfwSetWindowTitle(window, build_window_title(scene_kind_).c_str());
}

void simulation_state::on_exit(engine& engine) {
 if (scene_ && loading_feedback_presenter_ && loading_feedback_active_)
        loading_feedback_presenter_->on_loading_complete(engine, *scene_, scene_->get_scene_loader());
    loading_feedback_presenter_.reset();
    loading_feedback_active_ = false;
    cam_ = nullptr;
    release_scene_depth_texture();
    release_planetary_water_atlas_resources();
    scene_.reset();
}

void simulation_state::handle_input(engine& engine, float dt) {
    if (!cam_ || !scene_)
        return;

    constexpr std::array<int, 4> scene_switch_keys = {
        GLFW_KEY_F1,
        GLFW_KEY_F2,
        GLFW_KEY_F3,
        GLFW_KEY_F4
    };

    for (size_t i = 0; i < scene_switch_keys.size(); ++i) {
        if (poll_scene_switch_key(scene_switch_keys[i], previous_scene_switch_down_[i])) {
            switch_scene(engine, static_cast<example_scene_kind>(i));
            return;
        }
    }

    if (poll_toggle_key(GLFW_KEY_H, previous_terrain_debug_down_))
        terrain_debug_mode_ = (terrain_debug_mode_ + 1) % 6;

    if (poll_toggle_key(GLFW_KEY_B, previous_bounding_box_debug_down_))
        draw_bounding_boxes_ = !draw_bounding_boxes_;

    bool fluid_debug_mode_changed = false;
    if (poll_toggle_key(GLFW_KEY_J, previous_fluid_debug_next_down_)) {
        fluid_debug_mode_ = next_fluid_debug_mode(fluid_debug_mode_);
        fluid_debug_mode_changed = true;
    }
    if (poll_toggle_key(GLFW_KEY_K, previous_fluid_debug_prev_down_)) {
        fluid_debug_mode_ = previous_fluid_debug_mode(fluid_debug_mode_);
        fluid_debug_mode_changed = true;
    }
    if (fluid_debug_mode_changed) {
        for (auto* system : scene_->get_gpu_fluid_systems()) {
            if (system)
                system->set_debug_visualization_mode(fluid_debug_mode_);
        }

        std::cout << "[fluid_debug_mode] " << get_fluid_debug_mode_name(fluid_debug_mode_) << std::endl;
    }

    const bool left_mouse_down = input_system::is_button_down(GLFW_MOUSE_BUTTON_LEFT);
    const bool left_mouse_pressed = left_mouse_down && !previous_left_mouse_down_;
    previous_left_mouse_down_ = left_mouse_down;
    const bool escape_down = input_system::is_key_down(GLFW_KEY_ESCAPE);
    const bool escape_pressed = escape_down && !previous_escape_down_;
    previous_escape_down_ = escape_down;

    if (escape_pressed)
        detach_camera_parent();

    if (left_mouse_pressed && !input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT))
        try_begin_focus();

    const bool manual_camera_input = input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT)
        || input_system::is_key_down(GLFW_KEY_W)
        || input_system::is_key_down(GLFW_KEY_A)
        || input_system::is_key_down(GLFW_KEY_S)
        || input_system::is_key_down(GLFW_KEY_D);
    if (manual_camera_input)
        cancel_camera_focus();

    if (cam_)
        cam_->process_input(dt);
}

void simulation_state::fixed_update(engine& engine, float dt) {
    if (scene_)
        scene_->update();
}

void simulation_state::update(engine& engine, float dt) {
    if (!scene_)
        return;

    update_loading_feedback(engine);

    auto section = engine.get_frame_profiler().measure("scene_sync_render");
    scene_->sync_render();
    update_camera_focus(dt);
}

void simulation_state::render(engine& engine) {
    if (!cam_ || !scene_)
        return;

    auto& profiler = engine.get_frame_profiler();
    const glm::vec3 light_position = scene_->has_primary_light()
        ? scene_->get_primary_light_position()
        : cam_->get_transform()->get_global_position() + glm::vec3(0.f, 100.f, 100.f);
    const glm::vec3 light_color = scene_->has_primary_light()
        ? scene_->get_primary_light_color()
        : glm::vec3(1.0f, .8f, .3f);
    const float light_intensity = scene_->has_primary_light()
        ? scene_->get_primary_light_intensity()
        : 0.75f;

    {
        auto section = profiler.measure("render_clear");
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    {
        auto section = profiler.measure("render_pipeline_begin_frame");
        render_pipeline_.begin_frame();
    }

    {
        auto section = profiler.measure("render_pipeline_submit");
        for (auto* render : scene_->get_renderers())
            render_pipeline_.submit(render);
    }

    {
        auto section = profiler.measure("render_pipeline_flush");
        render_pipeline_.flush(cam_, scene_.get(), [&](shader& s) {
            s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
            s.set_uni_vec3("lightColor", light_color);
            s.set_uni_vec3("viewPos", cam_->get_transform()->get_global_position());
            s.set_uni_vec3("lightPos", light_position);
            s.set_uni_float("intensity", light_intensity);
            s.set_uni_int("terrainDebugMode", terrain_debug_mode_);
        });
    }

    bool needs_scene_depth_texture = false;
    for (auto* system : scene_->get_gpu_fluid_systems()) {
        if (system && system->requires_scene_depth_texture()) {
            needs_scene_depth_texture = true;
            break;
        }
    }

    if (needs_scene_depth_texture) {
        if (GLFWwindow* window = engine.get_window()) {
            int framebuffer_width = 0;
            int framebuffer_height = 0;
            glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
            if (framebuffer_width > 0 && framebuffer_height > 0)
                capture_scene_depth_texture(framebuffer_width, framebuffer_height);

            bool needs_particle_surface_pass = false;
            for (auto* system : scene_->get_gpu_fluid_systems()) {
                if (system && system->supports_particle_surface_pass()) {
                    needs_particle_surface_pass = true;
                    break;
                }
            }

            if (needs_particle_surface_pass) {
                ensure_planetary_water_atlas_targets(planetary_water_atlas_default_width, planetary_water_atlas_default_height);
            }
        }
    }

    for (auto* system : scene_->get_gpu_particle_systems()) {
        if (system)
            system->draw(cam_);
    }

    for (auto* system : scene_->get_gpu_fluid_systems()) {
        if (!system)
            continue;
        if (system->supports_particle_surface_pass()) {
            if (system->get_debug_visualization_mode() != fluid_debug_visualization_mode::none) {
                system->draw(cam_, system->requires_scene_depth_texture() ? scene_depth_texture_ : 0);
                continue;
            }

            render_planetary_water_atlas_input(*system);
            render_planetary_water_shell(*system, light_position, light_color, light_intensity);
            continue;
        }

        system->draw(cam_, system->requires_scene_depth_texture() ? scene_depth_texture_ : 0);
    }

    render_bounding_boxes(engine);

    if (loading_feedback_presenter_)
        loading_feedback_presenter_->render(engine, *scene_, scene_->get_scene_loader());
}

void simulation_state::initialize_bounding_box_debug_resources() {
    if (!scene_)
        return;

    auto& assets = scene_->get_asset_manager();
    if (!bounding_box_shader_)
        bounding_box_shader_ = assets.create_shader("debug.bounding_box", "GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");

    if (!bounding_box_mesh_) {
        static MeshData bounding_box_line_mesh = create_bounding_box_line_mesh();
        bounding_box_mesh_ = assets.create_mesh(bounding_box_line_mesh);
        if (bounding_box_mesh_)
            bounding_box_mesh_->type = MeshType::LINES;
    }
}

void simulation_state::render_bounding_boxes(engine& engine) {
    if (!draw_bounding_boxes_ || !scene_ || !cam_ || !bounding_box_shader_ || !bounding_box_mesh_)
        return;

    render_frame_context frame_context;
    if (GLFWwindow* window = engine.get_window()) {
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        const float aspect = framebuffer_height == 0 ? 1.0f : static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height);
        frame_context.projection = cam_->GetProjectionMatrix(aspect);
        frame_context.view = cam_->GetViewMatrix();
        frame_context.camera_position = cam_->get_node()->get_global_position();
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (auto* render : scene_->get_renderers()) {
        if (!render || !render->get_node())
            continue;

        const bounding_box world_bounds = render->get_node()->get_subtree_world_bounding_box();
        if (!world_bounds.valid)
            continue;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), world_bounds.get_center());
        model = glm::scale(model, world_bounds.get_size());

        bounding_box_shader_->use();
        bounding_box_shader_->set_uni_int("useInstancing", 0);
        bounding_box_shader_->set_uni_int("useGpuPositions", 0);
        bounding_box_shader_->set_uni_int("instanceBaseIndex", 0);
        bounding_box_shader_->set_uni_int("physicsBodyIndex", -1);
        bounding_box_shader_->set_uniform_mat4("view", frame_context.view);
        bounding_box_shader_->set_uniform_mat4("projection", frame_context.projection);
        bounding_box_shader_->set_uniform_mat4("model", model);
        bounding_box_shader_->set_uni_vec3("viewPos", frame_context.camera_position);
        bounding_box_shader_->set_uni_vec3("lightPos", frame_context.camera_position + glm::vec3(0.0f, 0.0f, 10.0f));
        bounding_box_shader_->set_uni_vec3("lightColor", glm::vec3(0.25f, 0.95f, 0.35f));
        bounding_box_shader_->set_uni_vec3("objectColor", glm::vec3(0.25f, 0.95f, 0.35f));
        bounding_box_mesh_->Draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void simulation_state::update_loading_feedback(engine& engine) {
    if (!scene_ || !loading_feedback_presenter_)
        return;

    auto& loader = scene_->get_scene_loader();
    if (!loader.is_started())
        return;

    if (!loading_feedback_active_) {
        loading_feedback_presenter_->on_loading_begin(engine, *scene_, loader);
        loading_feedback_active_ = true;
    }

    if (loader.is_failed()) {
        loading_feedback_presenter_->on_loading_failed(engine, *scene_, loader);
        loading_feedback_active_ = false;
        return;
    }

    if (loader.is_completed()) {
        loading_feedback_presenter_->on_loading_complete(engine, *scene_, loader);
        loading_feedback_active_ = false;
        return;
    }

    loading_feedback_presenter_->on_loading_update(engine, *scene_, loader);
}

std::unique_ptr<loading_feedback_presenter> simulation_state::create_loading_feedback_presenter(engine& engine) {
    return std::make_unique<window_title_loading_feedback>(build_window_title(scene_kind_));
}

void simulation_state::try_begin_focus() {
    if (!scene_ || !cam_)
        return;

    const auto pick = pick_renderer_from_mouse(*scene_, *cam_);
    if (!pick)
        return;

    const glm::vec3 current_camera_position = cam_->get_node()->get_global_position();
    glm::vec3 direction_from_target = current_camera_position - pick->world_position;
    if (glm::dot(direction_from_target, direction_from_target) <= 0.0001f)
        direction_from_target = -cam_->get_node()->forward();

    direction_from_target = glm::normalize(direction_from_target);
    const float focus_distance = std::max(18.f, pick->world_radius * 10.f);
    const glm::vec3 focus_target_world_position = pick->world_position
        + direction_from_target * focus_distance
        + glm::vec3(0.f, pick->world_radius * 1.25f, 0.f);

    focus_target_node_ = pick->render ? pick->render->get_node() : nullptr;
    if (!focus_target_node_)
        return;

    if (cam_->get_node()->get_parent() != scene_->get_root_node())
        cam_->get_node()->set_parent(scene_->get_root_node(), true);

    attached_camera_parent_ = nullptr;
    focus_start_position_ = cam_->get_node()->get_global_position();
    focus_target_position_ = focus_target_world_position;
    focus_target_offset_ = focus_target_position_;
    focus_look_at_ = pick->world_position;
    focus_elapsed_ = 0.f;
    focus_active_ = true;
}

void simulation_state::update_camera_focus(float dt) {
    if (!focus_active_ || !cam_)
        return;

    focus_elapsed_ += dt;
    if (focus_target_node_) {
        focus_look_at_ = focus_target_node_->get_global_position();
    }

    const float t = glm::clamp(focus_elapsed_ / focus_duration_, 0.f, 1.f);
    const float eased_t = ease_in_out_cubic(t);
    const glm::vec3 next_position = glm::mix(focus_start_position_, focus_target_position_, eased_t);

    cam_->get_node()->set_global_position(next_position);
    orient_camera_towards(*cam_, focus_look_at_);

   if (t >= 1.f) {
        focus_target_node_ = nullptr;
        focus_active_ = false;
   }
}

void simulation_state::cancel_camera_focus() {
    focus_active_ = false;
    focus_elapsed_ = 0.f;
    focus_target_node_ = nullptr;
}

void simulation_state::detach_camera_parent() {
    if (!cam_ || !scene_)
        return;

    cancel_camera_focus();
    if (attached_camera_parent_ || cam_->get_node()->get_parent() != scene_->get_root_node())
        cam_->get_node()->set_parent(scene_->get_root_node(), true);

    attached_camera_parent_ = nullptr;
}

void simulation_state::switch_scene(engine& engine, example_scene_kind next_scene_kind) {
    if (next_scene_kind == scene_kind_)
        return;

    engine.change_state(std::make_unique<simulation_state>(next_scene_kind));
}
