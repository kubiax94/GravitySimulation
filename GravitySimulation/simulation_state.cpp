#include "simulation_state.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/quaternion.hpp>

#include "cloth_scene.h"
#include "collision_debug_scene.h"
#include "collider.h"
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

bool wave_debug_next_down = false;
bool wave_debug_prev_down = false;

constexpr int wave_debug_mode_count = 12;

float build_debug_normalization_scale(float max_abs_value, float target_value, float max_scale) {
    if (max_abs_value <= 1e-9f)
        return 1.0f;

    return glm::clamp(target_value / max_abs_value, 1.0f, max_scale);
}

void log_planetary_tide_texture_stats(GLuint tidal_texture, int width, int height) {
    if (tidal_texture == 0 || width <= 0 || height <= 0)
        return;

    static int frame_counter = 0;
    ++frame_counter;
    if (frame_counter % 120 != 0)
        return;

    std::vector<float> readback(static_cast<size_t>(width) * static_cast<size_t>(height), 0.0f);
    glBindTexture(GL_TEXTURE_2D, tidal_texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, readback.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    float min_value = std::numeric_limits<float>::max();
    float max_value = std::numeric_limits<float>::lowest();
    float avg_abs = 0.0f;
    size_t active_count = 0u;
    for (const float value : readback) {
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
        avg_abs += std::abs(value);
        if (std::abs(value) > 1e-12f)
            ++active_count;
    }

    const float texel_count = static_cast<float>(std::max<size_t>(readback.size(), 1u));
    avg_abs /= texel_count;

    std::cout << "[planetary_tide_texture] min=" << min_value
        << " max=" << max_value
        << " avgAbs=" << avg_abs
        << " active=" << active_count << "/" << readback.size()
        << std::endl;
}

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

std::unique_ptr<scene> create_example_scene(simulation_state::example_scene_kind scene_kind, sim::time_sim* time) {
    switch (scene_kind) {
    case simulation_state::example_scene_kind::fluid:
        return std::make_unique<fluid_scene>(time);
    case simulation_state::example_scene_kind::cloth:
        return std::make_unique<cloth_scene>(time);
    case simulation_state::example_scene_kind::galactic:
        return std::make_unique<galactic_scene>(time);
    case simulation_state::example_scene_kind::galactic_stress:
        return std::make_unique<galactic_stress_scene>(time);
   case simulation_state::example_scene_kind::collision_debug:
        return std::make_unique<collision_debug_scene>(time);
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
   case simulation_state::example_scene_kind::collision_debug:
        return "Collision Debug";
    }

    return "Fluid";
}

std::string build_window_title(simulation_state::example_scene_kind scene_kind) {
    return std::string("GravitySimulation - ") + get_scene_name(scene_kind)
        + " [F1 Fluid | F2 Cloth | F3 Galactic | F4 Stress | F6 Collision]";
}

std::string build_window_title(simulation_state::example_scene_kind scene_kind, int terrain_debug_mode) {
    return build_window_title(scene_kind) + " [Dbg " + std::to_string(terrain_debug_mode) + "]";
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

MeshData create_line_segment_mesh() {
    MeshData data;
    Vertex start{};
    start.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    start.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    Vertex end{};
    end.Position = glm::vec3(0.0f, 0.0f, 1.0f);
    end.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    data.vertecies = { start, end };
    data.indices = { 0u, 1u };
    return data;
}

void draw_debug_mesh(shader& debug_shader, Mesh& debug_mesh, const render_frame_context& frame_context, const glm::mat4& model, const glm::vec3& color) {
    debug_shader.use();
    debug_shader.set_uni_int("useInstancing", 0);
    debug_shader.set_uni_int("useGpuPositions", 0);
    debug_shader.set_uni_int("instanceBaseIndex", 0);
    debug_shader.set_uni_int("physicsBodyIndex", -1);
    debug_shader.set_uniform_mat4("view", frame_context.view);
    debug_shader.set_uniform_mat4("projection", frame_context.projection);
    debug_shader.set_uniform_mat4("model", model);
    debug_shader.set_uni_vec3("viewPos", frame_context.camera_position);
    debug_shader.set_uni_vec3("lightPos", frame_context.camera_position + glm::vec3(0.0f, 0.0f, 10.0f));
    debug_shader.set_uni_vec3("lightColor", color);
    debug_shader.set_uni_vec3("objectColor", color);
    debug_mesh.Draw();
}

render_frame_context build_debug_frame_context(engine& engine, Camera& camera) {
    render_frame_context frame_context;
    if (GLFWwindow* window = engine.get_window()) {
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        const float aspect = framebuffer_height == 0 ? 1.0f : static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height);
        frame_context.projection = camera.GetProjectionMatrix(aspect);
        frame_context.view = camera.GetViewMatrix();
        frame_context.camera_position = camera.get_node()->get_global_position();
    }

    return frame_context;
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

std::optional<focus_pick_result> select_focus_pick_from_hits(
    const std::vector<ray_cast_hit>& hits,
    const glm::vec2& mouse_position,
    int window_width,
    int window_height,
    const glm::mat4& view,
    const glm::mat4& projection) {
    std::optional<focus_pick_result> best_pick;
    float best_score = std::numeric_limits<float>::max();

    for (const auto& hit : hits) {
        if (!hit.render)
            continue;

        const glm::vec3 world_position = hit.bounds.valid
            ? hit.bounds.get_center()
            : hit.render->get_node()->get_global_position();
        const float world_radius = estimate_renderer_radius(*hit.render);
        const glm::vec4 clip_position = projection * view * glm::vec4(world_position, 1.0f);
        if (clip_position.w <= 0.0001f)
            continue;

        const glm::vec3 ndc = glm::vec3(clip_position) / clip_position.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f)
            continue;

        const glm::vec2 screen_position(
            (ndc.x * 0.5f + 0.5f) * static_cast<float>(window_width),
            (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(window_height));
        const float screen_distance = glm::length(screen_position - mouse_position);

        const glm::vec4 center_view = view * glm::vec4(world_position, 1.0f);
        const float view_depth = glm::max(-center_view.z, 0.001f);
        const float projected_radius = projection[1][1] * world_radius * static_cast<float>(window_height) / view_depth;
        const float effective_radius = glm::max(projected_radius, 18.0f);
        const float score = screen_distance / effective_radius + hit.distance * 0.00035f;

        if (score >= best_score)
            continue;

        best_score = score;
        best_pick = focus_pick_result{
            hit.render,
            world_position,
            world_radius
        };
    }

    return best_pick;
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

    return select_focus_pick_from_hits(
        picker.get_hits(),
        glm::vec2(mouse_pos.x, mouse_pos.y),
        window_width,
        window_height,
        view,
        projection);
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

float smoothstep_scalar(float edge0, float edge1, float x) {
    const float denom = edge1 - edge0;
    if (std::abs(denom) <= 0.000001f)
        return x >= edge1 ? 1.0f : 0.0f;

    const float t = glm::clamp((x - edge0) / denom, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

size_t remap_wave_texel_to_domain_index(int x, int y, int wave_width, int wave_height, int domain_width, int domain_height) {
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(std::max(wave_width, 1));
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(std::max(wave_height, 1));
    const int domain_x = glm::clamp(static_cast<int>(std::floor(u * static_cast<float>(std::max(domain_width, 1)))), 0, std::max(domain_width - 1, 0));
    const int domain_y = glm::clamp(static_cast<int>(std::floor(glm::clamp(v, 0.0f, 1.0f) * static_cast<float>(std::max(domain_height, 1)))), 0, std::max(domain_height - 1, 0));
    return static_cast<size_t>(domain_y) * static_cast<size_t>(std::max(domain_width, 1)) + static_cast<size_t>(domain_x);
}

void log_planetary_wave_texture_stats(
    GLuint wave_texture,
    GLuint support_atlas_texture,
    GLuint tidal_texture,
    const planetary_water_domain& domain,
    int width,
    int height,
    const planetary_wave_update_context& update_context,
    bool enable_logging,
    float& out_height_scale,
    float& out_velocity_scale,
    float& out_tidal_scale) {
    if (wave_texture == 0 || support_atlas_texture == 0 || width <= 0 || height <= 0)
        return;

    static int frame_counter = 0;
    ++frame_counter;
    if (frame_counter % 30 != 0)
        return;

    const auto& desc = domain.get_desc();
    const auto& render_mask_data = domain.get_render_mask_data();
    const auto& water_level_data = domain.get_water_level_data();
    const auto& region_id_data = domain.get_region_id_data();
    const auto& shore_distance_data = domain.get_shore_distance_data();
    if (desc.width <= 0
        || desc.height <= 0
        || render_mask_data.empty()
        || water_level_data.empty()
        || region_id_data.empty()
        || shore_distance_data.empty())
        return;

    const int sample_width = std::min(width, 64);
    const int sample_height = std::min(height, 32);
    std::vector<float> readback(static_cast<size_t>(width) * static_cast<size_t>(height) * 2u, 0.0f);
    std::vector<float> atlas_readback(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0.0f);
    std::vector<float> tidal_readback(static_cast<size_t>(width) * static_cast<size_t>(height), 0.0f);
    glBindTexture(GL_TEXTURE_2D, wave_texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, readback.data());
    glBindTexture(GL_TEXTURE_2D, support_atlas_texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, atlas_readback.data());
    if (tidal_texture != 0) {
        glBindTexture(GL_TEXTURE_2D, tidal_texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, tidal_readback.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    float min_height = std::numeric_limits<float>::max();
    float max_height = std::numeric_limits<float>::lowest();
    float min_velocity = std::numeric_limits<float>::max();
    float max_velocity = std::numeric_limits<float>::lowest();
    float avg_abs_height = 0.0f;
    float avg_abs_velocity = 0.0f;
    float max_occupancy = 0.0f;
    float avg_occupancy = 0.0f;
    float max_carrier = 0.0f;
    float avg_carrier = 0.0f;
    float max_water_level = 0.0f;
    float avg_water_level = 0.0f;
    float min_shore_distance = std::numeric_limits<float>::max();
    float max_shore_distance = std::numeric_limits<float>::lowest();
    float avg_shore_distance = 0.0f;
    float max_abs_tidal = 0.0f;
    float avg_abs_tidal = 0.0f;
    float max_abs_forcing = 0.0f;
    float avg_abs_forcing = 0.0f;
    float min_damping = std::numeric_limits<float>::max();
    float max_damping = std::numeric_limits<float>::lowest();
    float avg_damping = 0.0f;
    size_t active_domain_texels = 0u;
    size_t active_atlas_texels = 0u;
    size_t energized_wave_texels = 0u;
    const size_t sample_count = static_cast<size_t>(sample_width) * static_cast<size_t>(sample_height);
    for (int y = 0; y < sample_height; ++y) {
        for (int x = 0; x < sample_width; ++x) {
            const size_t texel_index = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 2u;
            const float height_value = readback[texel_index + 0u];
            const float velocity_value = readback[texel_index + 1u];
        min_height = std::min(min_height, height_value);
        max_height = std::max(max_height, height_value);
        min_velocity = std::min(min_velocity, velocity_value);
        max_velocity = std::max(max_velocity, velocity_value);
        avg_abs_height += std::abs(height_value);
        avg_abs_velocity += std::abs(velocity_value);
        }
    }

    avg_abs_height /= static_cast<float>(sample_count);
    avg_abs_velocity /= static_cast<float>(sample_count);

    const size_t full_texel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t wave_texel_index = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x));
            const size_t wave_rg_index = wave_texel_index * 2u;
            const size_t atlas_rgba_index = wave_texel_index * 4u;
            const size_t domain_index = remap_wave_texel_to_domain_index(x, y, width, height, desc.width, desc.height);

            const float wave_height = readback[wave_rg_index + 0u];
            const float wave_velocity = readback[wave_rg_index + 1u];
            const float atlas_weight = glm::max(atlas_readback[atlas_rgba_index + 0u], 0.0f);
            const float depth01 = atlas_weight > 0.00001f ? glm::clamp(atlas_readback[atlas_rgba_index + 1u] / atlas_weight, 0.0f, 1.0f) : 0.0f;
            const float carrier = atlas_weight > 0.00001f ? glm::clamp(atlas_readback[atlas_rgba_index + 2u] / atlas_weight, 0.0f, 1.0f) : 0.0f;
            const float flood = atlas_weight > 0.00001f ? glm::clamp(atlas_readback[atlas_rgba_index + 3u] / atlas_weight, 0.0f, 1.0f) : 0.0f;
            const float occupancy = smoothstep_scalar(0.0012f, 0.020f, atlas_weight);
            const float water_domain = static_cast<float>(render_mask_data[domain_index]) / 255.0f;
            const float water_level = water_level_data[domain_index];
            const float shoreline_distance = shore_distance_data[domain_index];
            const unsigned short region_id = region_id_data[domain_index];
            const float tidal_height = tidal_texture != 0 ? tidal_readback[wave_texel_index] : 0.0f;
            const float support = occupancy * (0.55f + 0.45f * depth01) * (0.50f + 0.50f * flood);
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
            const float traveling_wave_a = std::sin((u * 17.0f + v * 9.0f) - update_context.time_seconds * 1.8f);
            const float traveling_wave_b = std::sin((u * -11.0f + v * 13.0f) + update_context.time_seconds * 1.2f);
            const float animated_forcing = traveling_wave_a * 0.65f + traveling_wave_b * 0.35f;
            const float shore_blend = glm::clamp(shoreline_distance / glm::max(update_context.shore_transition_distance, 0.0001f), 0.0f, 1.0f);
            const float forcing = ((glm::max(support, 0.0f) * 0.22f + glm::max(carrier, 0.0f) * 0.28f + 0.50f) * glm::max(water_level, 0.0f)
                + animated_forcing * 0.10f * glm::max(support, water_level)) * update_context.forcing_scale;
            const float damping = glm::mix(update_context.shore_damping, update_context.open_water_damping, shore_blend);

            if (water_domain > 0.001f && region_id != 0u)
                ++active_domain_texels;
            if (occupancy > 0.01f)
                ++active_atlas_texels;
            if (std::abs(wave_height) > 0.0005f || std::abs(wave_velocity) > 0.005f)
                ++energized_wave_texels;

            max_occupancy = std::max(max_occupancy, occupancy);
            avg_occupancy += occupancy;
            max_carrier = std::max(max_carrier, carrier);
            avg_carrier += carrier;
            max_water_level = std::max(max_water_level, water_level);
            avg_water_level += water_level;
            min_shore_distance = std::min(min_shore_distance, shoreline_distance);
            max_shore_distance = std::max(max_shore_distance, shoreline_distance);
            avg_shore_distance += shoreline_distance;
            max_abs_tidal = std::max(max_abs_tidal, std::abs(tidal_height));
            avg_abs_tidal += std::abs(tidal_height);
            max_abs_forcing = std::max(max_abs_forcing, std::abs(forcing));
            avg_abs_forcing += std::abs(forcing);
            min_damping = std::min(min_damping, damping);
            max_damping = std::max(max_damping, damping);
            avg_damping += damping;
        }
    }

    const float full_texel_count_f = static_cast<float>(std::max<size_t>(full_texel_count, 1u));
    avg_occupancy /= full_texel_count_f;
    avg_carrier /= full_texel_count_f;
    avg_water_level /= full_texel_count_f;
    avg_shore_distance /= full_texel_count_f;
    avg_abs_tidal /= full_texel_count_f;
    avg_abs_forcing /= full_texel_count_f;
    avg_damping /= full_texel_count_f;

    const float max_abs_height = std::max(std::abs(min_height), std::abs(max_height));
    const float max_abs_velocity = std::max(std::abs(min_velocity), std::abs(max_velocity));
    out_height_scale = build_debug_normalization_scale(max_abs_height, 0.85f, 250000.0f);
    out_velocity_scale = build_debug_normalization_scale(max_abs_velocity, 0.85f, 50000.0f);
    out_tidal_scale = build_debug_normalization_scale(max_abs_tidal, 0.90f, 100000000.0f);

    if (enable_logging) {
        std::cout << "[planetary_wave_debug] height[min=" << min_height
            << ", max=" << max_height
            << ", avgAbs=" << avg_abs_height
            << "] velocity[min=" << min_velocity
            << ", max=" << max_velocity
            << ", avgAbs=" << avg_abs_velocity
            << "] tidal[maxAbs=" << max_abs_tidal
            << ", avgAbs=" << avg_abs_tidal
            << "] active[domain=" << active_domain_texels << "/" << full_texel_count
            << ", atlas=" << active_atlas_texels << "/" << full_texel_count
            << ", energized=" << energized_wave_texels << "/" << full_texel_count
            << "] occupancy[max=" << max_occupancy
            << ", avg=" << avg_occupancy
            << "] carrier[max=" << max_carrier
            << ", avg=" << avg_carrier
            << "] waterLevel[max=" << max_water_level
            << ", avg=" << avg_water_level
            << "] shore[min=" << min_shore_distance
            << ", max=" << max_shore_distance
            << ", avg=" << avg_shore_distance
            << "] forcing[avgAbs=" << avg_abs_forcing
            << ", maxAbs=" << max_abs_forcing
            << "] damping[min=" << min_damping
            << ", max=" << max_damping
            << ", avg=" << avg_damping
            << "]" << std::endl;
    }
}
}

simulation_state::simulation_state(std::unique_ptr<scene> scene)
    : scene_(std::move(scene)) {
}

simulation_state::simulation_state(example_scene_kind scene_kind)
    : scene_kind_(scene_kind) {
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
    static MeshData fullscreen_quad_mesh_data = create_fullscreen_quad_mesh();
    if (!particle_surface_composite_mesh_)
        particle_surface_composite_mesh_ = assets.create_mesh(fullscreen_quad_mesh_data);
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


void simulation_state::on_enter(engine& engine) {
    if (!scene_)
        scene_ = create_example_scene(scene_kind_, &engine.get_time());

    engine.get_time().reset(static_cast<float>(glfwGetTime()));

    loading_feedback_presenter_ = create_loading_feedback_presenter(engine);
    loading_feedback_active_ = false;
    scene_->init();
    scene_->initialize_runtime_resources();
    initialize_bounding_box_debug_resources();
    initialize_particle_surface_composite_resources();
    cam_ = scene_->get_main_camera();
    update_loading_feedback(engine);

    for (auto* system : scene_->get_gpu_fluid_systems()) {
        if (system)
            system->set_debug_visualization_mode(fluid_debug_mode_);
    }

    if (GLFWwindow* window = engine.get_window())
        glfwSetWindowTitle(window, build_window_title(scene_kind_, terrain_debug_mode_).c_str());
}

void simulation_state::on_exit(engine& engine) {
 if (scene_ && loading_feedback_presenter_ && loading_feedback_active_)
        loading_feedback_presenter_->on_loading_complete(engine, *scene_, scene_->get_scene_loader());
    loading_feedback_presenter_.reset();
    loading_feedback_active_ = false;
    focus_active_ = false;
    focus_elapsed_ = 0.f;
    focus_target_node_ = nullptr;
    attached_camera_parent_ = nullptr;
    cam_ = nullptr;
    engine.get_ui().shutdown();
    render_pipeline_.reset_cache();
    render_pipeline_.release_scene_depth_texture();
    scene_->release_runtime_resources();
    scene_.reset();
}

void simulation_state::handle_input(engine& engine, float dt) {
    if (!cam_ || !scene_)
        return;

    constexpr std::array<int, 5> scene_switch_keys = {
        GLFW_KEY_F1,
        GLFW_KEY_F2,
        GLFW_KEY_F3,
        GLFW_KEY_F4,
     GLFW_KEY_F6
    };

    for (size_t i = 0; i < scene_switch_keys.size(); ++i) {
        if (poll_scene_switch_key(scene_switch_keys[i], previous_scene_switch_down_[i])) {
            switch_scene(engine, static_cast<example_scene_kind>(i));
            return;
        }
    }

    if (scene_kind_ != example_scene_kind::fluid && scene_kind_ != example_scene_kind::galactic && poll_toggle_key(GLFW_KEY_H, previous_terrain_debug_down_)) {
        terrain_debug_mode_ = (terrain_debug_mode_ + 1) % 11;
        std::cout << "[terrain_debug_mode] " << terrain_debug_mode_ << std::endl;
        if (GLFWwindow* window = engine.get_window())
            glfwSetWindowTitle(window, build_window_title(scene_kind_, terrain_debug_mode_).c_str());
    }

    scene_->handle_input(engine, dt);

    if (poll_toggle_key(GLFW_KEY_B, previous_bounding_box_debug_down_))
        draw_bounding_boxes_ = !draw_bounding_boxes_;

    if (poll_toggle_key(GLFW_KEY_N, previous_collision_debug_down_))
        draw_collision_debug_ = !draw_collision_debug_;

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
                render_pipeline_.capture_scene_depth_texture(framebuffer_width, framebuffer_height);
        }
    }

    for (auto* system : scene_->get_gpu_particle_systems()) {
        if (system)
            system->draw(cam_);
    }

    scene_render_context scene_context{
        render_pipeline_,
        *cam_,
        light_position,
        light_color,
        light_intensity,
        scene_kind_ == example_scene_kind::galactic
            ? (dynamic_cast<galactic_scene*>(scene_.get()) ? dynamic_cast<galactic_scene*>(scene_.get())->get_wave_debug_mode() : terrain_debug_mode_)
            : terrain_debug_mode_,
        0,
        0
    };
    if (GLFWwindow* window = engine.get_window())
        glfwGetFramebufferSize(window, &scene_context.framebuffer_width, &scene_context.framebuffer_height);

    for (auto* system : scene_->get_gpu_fluid_systems()) {
        if (!system)
            continue;

        if (scene_->render_fluid_system(engine, scene_context, *system))
            continue;

        system->draw(cam_, system->requires_scene_depth_texture() ? render_pipeline_.get_scene_depth_texture_id() : 0);
    }

    scene_->render_runtime(engine, scene_context);

    render_bounding_boxes(engine);
    render_collision_debug(engine);

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

    if (!collision_contact_mesh_) {
        static MeshData collision_contact_line_mesh = create_line_segment_mesh();
        collision_contact_mesh_ = assets.create_mesh(collision_contact_line_mesh);
        if (collision_contact_mesh_)
            collision_contact_mesh_->type = MeshType::LINES;
    }
}

void simulation_state::render_bounding_boxes(engine& engine) {
    if (!draw_bounding_boxes_ || !scene_ || !cam_ || !bounding_box_shader_ || !bounding_box_mesh_)
        return;

    const render_frame_context frame_context = build_debug_frame_context(engine, *cam_);

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

        draw_debug_mesh(*bounding_box_shader_, *bounding_box_mesh_, frame_context, model, glm::vec3(0.25f, 0.95f, 0.35f));
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void simulation_state::render_collision_debug(engine& engine) {
    if (!draw_collision_debug_ || !scene_ || !cam_ || !bounding_box_shader_ || !bounding_box_mesh_ || !collision_contact_mesh_)
        return;

    const render_frame_context frame_context = build_debug_frame_context(engine, *cam_);

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (auto* collider_component : scene_->get_colliders()) {
        if (!collider_component || !collider_component->is_enabled())
            continue;

        const bounding_box world_bounds = collider_component->get_world_bounds();
        if (!world_bounds.valid)
            continue;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), world_bounds.get_center());
        model = glm::scale(model, world_bounds.get_size());
        const glm::vec3 color = collider_component->is_trigger()
            ? glm::vec3(0.95f, 0.75f, 0.2f)
            : glm::vec3(0.2f, 0.75f, 0.95f);
        draw_debug_mesh(*bounding_box_shader_, *bounding_box_mesh_, frame_context, model, color);
    }

    for (const auto& contact : scene_->get_solid_collision_contacts()) {
        if (!contact.is_valid())
            continue;

        const glm::vec3 start = contact.overlap_bounds.get_center();
        const float line_length = glm::max(contact.penetration_depth, 0.001f);
        const glm::vec3 end = start + contact.normal * line_length;
        const glm::vec3 delta = end - start;
        const float scale = glm::length(delta);
        if (scale <= 0.000001f)
            continue;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), start);
        const glm::vec3 direction = delta / scale;
        const glm::vec3 up = std::abs(glm::dot(direction, glm::vec3(0.f, 1.f, 0.f))) > 0.999f
            ? glm::vec3(1.f, 0.f, 0.f)
            : glm::vec3(0.f, 1.f, 0.f);
        const glm::vec3 tangent = glm::normalize(glm::cross(up, direction));
        const glm::vec3 bitangent = glm::normalize(glm::cross(direction, tangent));
        model[0] = glm::vec4(tangent * 0.05f, 0.f);
        model[1] = glm::vec4(bitangent * 0.05f, 0.f);
        model[2] = glm::vec4(direction * scale, 0.f);
        draw_debug_mesh(*bounding_box_shader_, *collision_contact_mesh_, frame_context, model, glm::vec3(0.95f, 0.2f, 0.35f));
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
    return std::make_unique<runtime_ui_loading_feedback>();

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

