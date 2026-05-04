#include "galactic_scene.h"

#include "galactic_simulation_test.h"

#include "Camera.h"
#include "engine.h"
#include "g_shape.h"
#include "gpu_fluid_system_component.h"
#include "gpu_particle_system_component.h"
#include "planetary_water_render_resource.h"
#include "render_pipeline.h"
#include "frame_profiler.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>

namespace {
constexpr glm::vec3 initial_camera_position(2250.f, 650.f, 5200.f);
constexpr glm::vec3 initial_camera_rotation(-7.f, 0.f, 0.f);
constexpr float axial_rotation_base_speed = 0.002f;
constexpr int background_star_count = 2200;
constexpr int background_galaxy_count = 42;
constexpr float background_star_min_radius = 18000.f;
constexpr float background_star_max_radius = 42000.f;
constexpr float background_galaxy_min_radius = 26000.f;
constexpr float background_galaxy_max_radius = 46000.f;
constexpr int planetary_water_atlas_default_width = 2048;
constexpr int planetary_water_atlas_default_height = 1024;

constexpr float planet_spin_speed_multipliers[] = {
    0.017f,
    -0.004f,
    1.0f,
    0.973f,
    2.414f,
    2.245f,
    -1.392f,
    1.49f,
    0.036f
};

constexpr const char* planet_spin_node_names[] = {
    "Mercury_visual_spin",
    "Venus_visual_spin",
    "Earth_visual_spin",
    "Mars_visual_spin",
    "Jupiter_visual_spin",
    "Saturn_visual_spin",
    "Uranus_visual_spin",
    "Neptune_visual_spin",
    "Moon_visual_spin"
};

MeshData create_particle_point_mesh() {
    MeshData data;
    Vertex vertex{};
    vertex.Position = glm::vec3(0.f);
    vertex.Normal = glm::vec3(0.f, 1.f, 0.f);
    data.vertecies.push_back(vertex);
    data.indices = { 0u };
    return data;
}

std::vector<physics_data> create_background_particles(int count, float min_radius, float max_radius, uint32_t seed, float band_bias) {
    std::vector<physics_data> particles;
    particles.reserve(static_cast<size_t>(count));

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit_dist(0.f, 1.f);
    std::uniform_real_distribution<float> angle_dist(0.f, glm::two_pi<float>());

    for (int i = 0; i < count; ++i) {
        const float band_mix = unit_dist(rng);
        const float z = glm::mix(
            unit_dist(rng) * 2.0f - 1.0f,
            glm::clamp((unit_dist(rng) * 2.0f - 1.0f) * band_bias, -1.0f, 1.0f),
            glm::clamp(band_mix * 0.75f + 0.25f, 0.0f, 1.0f));
        const float azimuth = angle_dist(rng);
        const float xy = std::sqrt(glm::max(0.0f, 1.0f - z * z));
        const glm::vec3 normal(
            std::cos(azimuth) * xy,
            z,
            std::sin(azimuth) * xy);
        const float radius = glm::mix(min_radius, max_radius, std::pow(unit_dist(rng), 0.82f));

        physics_data particle;
        particle.position = glm::vec4(normal * radius, 1.0f);
        particle.velocity = glm::vec4(0.f, 0.f, 0.f, 1.0f);
        particle.accumulated_force = glm::vec4(0.f, 0.f, 0.f, 1.0f);
        particles.push_back(particle);
    }

    return particles;
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

float build_debug_normalization_scale(float max_abs_value, float target_value, float max_scale) {
    if (max_abs_value <= 1e-9f)
        return 1.0f;

    return glm::clamp(target_value / max_abs_value, 1.0f, max_scale);
}

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

void log_planetary_tide_texture_stats(GLuint tidal_texture, int width, int height) {
    if (tidal_texture == 0 || width <= 0 || height <= 0)
        return;

    constexpr int report_interval_frames = 120;
    static int frame_counter = 0;
    ++frame_counter;
    if (frame_counter < report_interval_frames)
        return;

    frame_counter = 0;

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

    frame_profiler::add_value_active("planetary_tide_texture_min", min_value);
    frame_profiler::add_value_active("planetary_tide_texture_max", max_value);
    frame_profiler::add_value_active("planetary_tide_texture_avg_abs", avg_abs);
    frame_profiler::add_value_active("planetary_tide_texture_active_ratio", static_cast<double>(active_count) / static_cast<double>(std::max<size_t>(readback.size(), 1u)));
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

    if (!enable_logging)
        return;

    constexpr int report_interval_frames = 120;
    static int frame_counter = 0;
    ++frame_counter;
    if (frame_counter < report_interval_frames)
        return;

    frame_counter = 0;

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
    float max_abs_tidal = 0.0f;
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
            const float flood = atlas_weight > 0.00001f ? glm::clamp(atlas_readback[atlas_rgba_index + 3u] / atlas_weight, 0.0f, 1.0f) : 0.0f;
            const float occupancy = smoothstep_scalar(0.0012f, 0.020f, atlas_weight);
            const float water_domain = static_cast<float>(render_mask_data[domain_index]) / 255.0f;
            const unsigned short region_id = region_id_data[domain_index];
            const float shoreline_distance = shore_distance_data[domain_index];
            const float tidal_height = tidal_texture != 0 ? tidal_readback[wave_texel_index] : 0.0f;
            const float support = occupancy * (0.55f + 0.45f * depth01) * (0.50f + 0.50f * flood);
            const float shore_blend = glm::clamp(shoreline_distance / glm::max(update_context.shore_transition_distance, 0.0001f), 0.0f, 1.0f);
            const float damping = glm::mix(update_context.shore_damping, update_context.open_water_damping, shore_blend);

            if (water_domain > 0.001f && region_id != 0u)
                ++active_domain_texels;
            if (occupancy > 0.01f)
                ++active_atlas_texels;
            if (std::abs(wave_height) > 0.0005f || std::abs(wave_velocity) > 0.005f)
                ++energized_wave_texels;

            max_abs_tidal = std::max(max_abs_tidal, std::abs(tidal_height));
            (void)support;
            (void)damping;
        }
    }

    const float max_abs_height = std::max(std::abs(min_height), std::abs(max_height));
    const float max_abs_velocity = std::max(std::abs(min_velocity), std::abs(max_velocity));
    out_height_scale = build_debug_normalization_scale(max_abs_height, 0.85f, 250000.0f);
    out_velocity_scale = build_debug_normalization_scale(max_abs_velocity, 0.85f, 50000.0f);
    out_tidal_scale = build_debug_normalization_scale(max_abs_tidal, 0.90f, 100000000.0f);

    frame_profiler::add_value_active("planetary_wave_debug_height_min", min_height);
    frame_profiler::add_value_active("planetary_wave_debug_height_max", max_height);
    frame_profiler::add_value_active("planetary_wave_debug_height_avg_abs", avg_abs_height);
    frame_profiler::add_value_active("planetary_wave_debug_velocity_min", min_velocity);
    frame_profiler::add_value_active("planetary_wave_debug_velocity_max", max_velocity);
    frame_profiler::add_value_active("planetary_wave_debug_velocity_avg_abs", avg_abs_velocity);
    frame_profiler::add_value_active("planetary_wave_debug_tidal_max_abs", max_abs_tidal);
    frame_profiler::add_value_active("planetary_wave_debug_active_domain_ratio", static_cast<double>(active_domain_texels) / static_cast<double>(std::max<size_t>(full_texel_count, 1u)));
    frame_profiler::add_value_active("planetary_wave_debug_active_atlas_ratio", static_cast<double>(active_atlas_texels) / static_cast<double>(std::max<size_t>(full_texel_count, 1u)));
    frame_profiler::add_value_active("planetary_wave_debug_energized_ratio", static_cast<double>(energized_wave_texels) / static_cast<double>(std::max<size_t>(full_texel_count, 1u)));
}
}

galactic_scene::galactic_scene(sim::time_sim* time)
    : scene(time) {
    initialize_scene_content();
}

void galactic_scene::initialize_scene_content() {
    set_simulation_speed(3600.f);

    auto& assets = get_asset_manager();

    camera_node_ = create_scene_node("galactic_cam");
    camera_node_->add_component<Camera>(camera_node_);
    camera_node_->set_global_position(initial_camera_position);
    camera_node_->set_global_rotation(initial_camera_rotation);

    simtest::init_gravity_test(this, planet_renderers_);

    background_star_node_ = create_scene_node("galactic_background_stars");
    background_galaxy_node_ = create_scene_node("galactic_background_galaxies");
    static MeshData particle_data = create_particle_point_mesh();
    auto* particle_mesh = assets.create_mesh(particle_data);
    particle_mesh->type = MeshType::POINTS;
    auto* particle_shader = assets.create_shader(
        "galactic.background.particles",
        "GravitySimulation/gpu_particle_system.vs.shader",
        "GravitySimulation/gpu_particle_system.fs.shader");
    auto* background_compute_stars = assets.create_compute_shader(
        "galactic.background.compute.stars",
        "GravitySimulation/cosmic_background_particles.glsl");
    auto* background_compute_galaxies = assets.create_compute_shader(
        "galactic.background.compute.galaxies",
        "GravitySimulation/cosmic_background_particles.glsl");

    auto* stars = background_star_node_->add_component<gpu_particle_system_component>(
        background_star_node_,
        background_compute_stars,
        particle_shader,
        particle_mesh,
        get_unit_system(),
        create_background_particles(background_star_count, background_star_min_radius, background_star_max_radius, 0x51A7BEEFu, 0.26f),
        1.45f,
        1.0f);
    stars->set_particle_color(glm::vec3(0.96f, 0.98f, 1.0f));
    stars->set_particle_alpha(0.88f);
    stars->set_particle_glow_strength(1.15f);
    stars->set_particle_size_jitter(1.0f);
    stars->set_particle_visual_mode(1);

    auto* galaxies = background_galaxy_node_->add_component<gpu_particle_system_component>(
        background_galaxy_node_,
        background_compute_galaxies,
        particle_shader,
        particle_mesh,
        get_unit_system(),
        create_background_particles(background_galaxy_count, background_galaxy_min_radius, background_galaxy_max_radius, 0x0A11CE42u, 0.68f),
        18.0f,
        1.0f);
    galaxies->set_particle_color(glm::vec3(0.78f, 0.86f, 1.0f));
    galaxies->set_particle_alpha(0.34f);
    galaxies->set_particle_glow_strength(1.45f);
    galaxies->set_particle_size_jitter(0.35f);
    galaxies->set_particle_visual_mode(2);
}

planetary_water_render_resource* galactic_scene::get_water_render_resource() const {
    return runtime_resources_.get<planetary_water_render_resource>(runtime_resource_key::planetary_water_render);
}

shader* galactic_scene::get_particle_surface_composite_shader() const {
    return runtime_resources_.get<shader>(runtime_resource_key::particle_surface_composite_shader);
}

Mesh* galactic_scene::get_particle_surface_composite_mesh() const {
    return runtime_resources_.get<Mesh>(runtime_resource_key::particle_surface_composite_mesh);
}

shader* galactic_scene::get_water_atlas_shader() const {
    return runtime_resources_.get<shader>(runtime_resource_key::planetary_water_atlas_shader);
}

shader* galactic_scene::get_water_atlas_blur_shader() const {
    return runtime_resources_.get<shader>(runtime_resource_key::planetary_water_atlas_blur_shader);
}

shader* galactic_scene::get_water_atlas_temporal_shader() const {
    return runtime_resources_.get<shader>(runtime_resource_key::planetary_water_atlas_temporal_shader);
}

shader* galactic_scene::get_water_shell_shader() const {
    return runtime_resources_.get<shader>(runtime_resource_key::planetary_water_shell_shader);
}

shader* galactic_scene::get_wave_debug_shell_shader() const {
    return runtime_resources_.get<shader>(runtime_resource_key::planetary_wave_debug_shell_shader);
}

compute_shader* galactic_scene::get_wave_propagation_shader() const {
    return runtime_resources_.get<compute_shader>(runtime_resource_key::planetary_wave_propagation_shader);
}

compute_shader* galactic_scene::get_wave_render_filter_shader() const {
    return runtime_resources_.get<compute_shader>(runtime_resource_key::planetary_wave_render_filter_shader);
}

compute_shader* galactic_scene::get_tide_field_shader() const {
    return runtime_resources_.get<compute_shader>(runtime_resource_key::planetary_tide_field_shader);
}

Mesh* galactic_scene::get_water_shell_mesh() const {
    return runtime_resources_.get<Mesh>(runtime_resource_key::planetary_water_shell_mesh);
}

void galactic_scene::initialize_runtime_resources() {
    auto& assets = get_asset_manager();
    if (!runtime_resources_.get<planetary_water_render_resource>(runtime_resource_key::planetary_water_render))
        runtime_resources_.set(runtime_resource_key::planetary_water_render, assets.create_planetary_water_render_resource("galactic.planetary.water.render.targets"));
    if (!runtime_resources_.get<shader>(runtime_resource_key::particle_surface_composite_shader)) {
        runtime_resources_.set(runtime_resource_key::particle_surface_composite_shader, assets.create_shader(
            "particle.surface.composite",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/particle_surface_composite.fs.shader"));
    }
    if (!runtime_resources_.get<shader>(runtime_resource_key::particle_surface_blur_shader)) {
        runtime_resources_.set(runtime_resource_key::particle_surface_blur_shader, assets.create_shader(
            "particle.surface.blur",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/particle_surface_blur.fs.shader"));
    }
    if (!runtime_resources_.get<shader>(runtime_resource_key::planetary_water_atlas_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_water_atlas_shader, assets.create_shader(
            "planetary.water.atlas.input",
            "GravitySimulation/planetary_water_atlas_input.vs.shader",
            "GravitySimulation/planetary_water_atlas_input.fs.shader"));
    }
    if (!runtime_resources_.get<shader>(runtime_resource_key::planetary_water_atlas_blur_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_water_atlas_blur_shader, assets.create_shader(
            "planetary.water.atlas.blur",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/planetary_water_atlas_blur.fs.shader"));
    }
    if (!runtime_resources_.get<shader>(runtime_resource_key::planetary_water_atlas_temporal_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_water_atlas_temporal_shader, assets.create_shader(
            "planetary.water.atlas.temporal",
            "GravitySimulation/particle_surface_composite.vs.shader",
            "GravitySimulation/planetary_water_atlas_temporal.fs.shader"));
    }
    if (!runtime_resources_.get<shader>(runtime_resource_key::planetary_water_shell_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_water_shell_shader, assets.create_shader(
            "planetary.water.shell",
            "GravitySimulation/planetary_water_shell.vs.shader",
            "GravitySimulation/planetary_water_shell.fs.shader"));
    }
    if (!runtime_resources_.get<shader>(runtime_resource_key::planetary_wave_debug_shell_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_wave_debug_shell_shader, assets.create_shader(
            "planetary.water.wave.debug.shell",
            "GravitySimulation/planetary_water_shell.vs.shader",
            "GravitySimulation/planetary_wave_debug_shell.fs.shader"));
    }
    if (!runtime_resources_.get<compute_shader>(runtime_resource_key::planetary_wave_propagation_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_wave_propagation_shader, assets.create_compute_shader(
            "planetary.water.wave.propagation",
            "GravitySimulation/planetary_wave_propagation.glsl"));
    }
    if (!runtime_resources_.get<compute_shader>(runtime_resource_key::planetary_wave_render_filter_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_wave_render_filter_shader, assets.create_compute_shader(
            "planetary.water.wave.render.filter",
            "GravitySimulation/planetary_wave_render_filter.glsl"));
    }
    if (!runtime_resources_.get<compute_shader>(runtime_resource_key::planetary_tide_field_shader)) {
        runtime_resources_.set(runtime_resource_key::planetary_tide_field_shader, assets.create_compute_shader(
            "planetary.water.tide.field",
            "GravitySimulation/planetary_tide_field.glsl"));
    }
    if (auto* propagation_shader = get_wave_propagation_shader())
        planetary_wave_field_.initialize(propagation_shader, get_wave_render_filter_shader());
    static MeshData fullscreen_quad_mesh_data = create_fullscreen_quad_mesh();
    if (!runtime_resources_.get<Mesh>(runtime_resource_key::particle_surface_composite_mesh))
        runtime_resources_.set(runtime_resource_key::particle_surface_composite_mesh, assets.create_mesh(fullscreen_quad_mesh_data));
    static MeshData planetary_water_shell_mesh_data = g_shape::generate_sphere(1.0f, 192, 96);
    if (!runtime_resources_.get<Mesh>(runtime_resource_key::planetary_water_shell_mesh))
        runtime_resources_.set(runtime_resource_key::planetary_water_shell_mesh, assets.create_mesh(planetary_water_shell_mesh_data));
}

void galactic_scene::release_runtime_resources() {
    release_planetary_water_atlas_resources();
    runtime_resources_.clear();
}

void galactic_scene::ensure_planetary_water_atlas_targets(int width, int height) {
    auto* render_resource = get_water_render_resource();
    if (!render_resource)
        return;

    render_resource->ensure_atlas_targets(width, height);
}

void galactic_scene::release_planetary_water_atlas_resources() {
    if (auto* render_resource = get_water_render_resource())
        render_resource->release_atlas_targets();

    planetary_wave_field_.reset();
}

void galactic_scene::blur_planetary_water_atlas(const gpu_fluid_system_component& system) {
    auto* blur_shader = get_water_atlas_blur_shader();
    auto* composite_mesh = get_particle_surface_composite_mesh();
    auto* render_resource = get_water_render_resource();
    if (!blur_shader || !composite_mesh || !render_resource)
        return;
    if (!active_render_pipeline_)
        return;

    auto& atlas_targets = render_resource->get_atlas_targets();
    if (atlas_targets.framebuffer == 0 || atlas_targets.atlas_texture.get_id() == 0 || atlas_targets.atlas_ping_texture.get_id() == 0)
        return;

    const auto& domain_textures = system.get_planetary_water_domain().get_textures();

    const std::vector<render_pipeline::offscreen_attachment> color_attachments = {
        { GL_COLOR_ATTACHMENT0, atlas_targets.atlas_texture.get_id() }
    };
    if (!active_render_pipeline_->begin_offscreen_pass(atlas_targets.framebuffer, atlas_targets.width, atlas_targets.height, color_attachments))
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    blur_shader->use();
    blur_shader->set_uni_int("inputTexture", 0);
    blur_shader->set_uni_int("waterDomainTextureAvailable", domain_textures.render_mask_texture != 0 ? 1 : 0);
    blur_shader->set_uni_int("waterDomainTexture", 1);
    const auto blur_masked_texture = [&](GLuint source_texture, GLuint ping_texture, float blur_radius_scale) {
        if (source_texture == 0 || ping_texture == 0)
            return;

        blur_shader->set_uni_float("blurRadiusScale", blur_radius_scale);
        active_render_pipeline_->set_offscreen_color_attachment(GL_COLOR_ATTACHMENT0, ping_texture);
        active_render_pipeline_->set_offscreen_draw_attachments({ GL_COLOR_ATTACHMENT0 });
        blur_shader->set_uni_vec2("blurDirection", glm::vec2(1.0f, 0.0f));
        active_render_pipeline_->draw_fullscreen_pass({
            *blur_shader,
            *composite_mesh,
            {
                { 0u, GL_TEXTURE_2D, source_texture },
                { 1u, GL_TEXTURE_2D, domain_textures.render_mask_texture }
            }
        });

        active_render_pipeline_->set_offscreen_color_attachment(GL_COLOR_ATTACHMENT0, source_texture);
        active_render_pipeline_->set_offscreen_draw_attachments({ GL_COLOR_ATTACHMENT0 });
        blur_shader->set_uni_vec2("blurDirection", glm::vec2(0.0f, 1.0f));
        active_render_pipeline_->draw_fullscreen_pass({
            *blur_shader,
            *composite_mesh,
            {
                { 0u, GL_TEXTURE_2D, ping_texture },
                { 1u, GL_TEXTURE_2D, domain_textures.render_mask_texture }
            }
        });
    };

    blur_masked_texture(atlas_targets.atlas_texture.get_id(), atlas_targets.atlas_ping_texture.get_id(), 1.72f);
    blur_masked_texture(atlas_targets.wave_forcing_texture.get_id(), atlas_targets.wave_forcing_ping_texture.get_id(), 1.42f);

    active_render_pipeline_->end_offscreen_pass();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void galactic_scene::stabilize_planetary_water_atlas(const gpu_fluid_system_component& system) {
    auto* temporal_shader = get_water_atlas_temporal_shader();
    auto* composite_mesh = get_particle_surface_composite_mesh();
    auto* render_resource = get_water_render_resource();
    if (!temporal_shader || !composite_mesh || !render_resource)
        return;
    if (!active_render_pipeline_)
        return;

    auto& atlas_targets = render_resource->get_atlas_targets();
    if (atlas_targets.framebuffer == 0 || atlas_targets.atlas_texture.get_id() == 0 || atlas_targets.atlas_ping_texture.get_id() == 0 || atlas_targets.atlas_history_texture.get_id() == 0)
        return;

    const auto& domain_textures = system.get_planetary_water_domain().get_textures();

    const std::vector<render_pipeline::offscreen_attachment> color_attachments = {
        { GL_COLOR_ATTACHMENT0, atlas_targets.atlas_ping_texture.get_id() }
    };
    if (!active_render_pipeline_->begin_offscreen_pass(atlas_targets.framebuffer, atlas_targets.width, atlas_targets.height, color_attachments))
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    temporal_shader->use();
    temporal_shader->set_uni_int("currentAtlasTexture", 0);
    temporal_shader->set_uni_int("historyAtlasTexture", 1);
    temporal_shader->set_uni_int("waterDomainTextureAvailable", domain_textures.render_mask_texture != 0 ? 1 : 0);
    temporal_shader->set_uni_int("waterDomainTexture", 2);

    const auto stabilize_masked_texture = [&](GLuint current_texture, GLuint ping_texture, GLuint history_texture, float history_blend) {
        if (current_texture == 0 || ping_texture == 0 || history_texture == 0)
            return;

        active_render_pipeline_->set_offscreen_color_attachment(GL_COLOR_ATTACHMENT0, ping_texture);
        active_render_pipeline_->set_offscreen_draw_attachments({ GL_COLOR_ATTACHMENT0 });
        temporal_shader->set_uni_float("historyBlend", history_blend);

        active_render_pipeline_->draw_fullscreen_pass({
            *temporal_shader,
            *composite_mesh,
            {
                { 0u, GL_TEXTURE_2D, current_texture },
                { 1u, GL_TEXTURE_2D, history_texture },
                { 2u, GL_TEXTURE_2D, domain_textures.render_mask_texture }
            }
        });

        active_render_pipeline_->copy_texture_2d(ping_texture, current_texture, atlas_targets.width, atlas_targets.height);
        active_render_pipeline_->copy_texture_2d(ping_texture, history_texture, atlas_targets.width, atlas_targets.height);
    };

    stabilize_masked_texture(atlas_targets.atlas_texture.get_id(), atlas_targets.atlas_ping_texture.get_id(), atlas_targets.atlas_history_texture.get_id(), 0.82f);
    stabilize_masked_texture(atlas_targets.wave_forcing_texture.get_id(), atlas_targets.wave_forcing_ping_texture.get_id(), atlas_targets.wave_forcing_history_texture.get_id(), 0.76f);

    active_render_pipeline_->end_offscreen_pass();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void galactic_scene::render_planetary_water_atlas_input(const gpu_fluid_system_component& system) {
    auto* atlas_shader = get_water_atlas_shader();
    auto* render_resource = get_water_render_resource();
    if (!atlas_shader || !render_resource || !active_render_pipeline_)
        return;

    auto& atlas_targets = render_resource->get_atlas_targets();
    if (atlas_targets.framebuffer == 0 || atlas_targets.atlas_texture.get_id() == 0 || atlas_targets.wave_forcing_texture.get_id() == 0)
        return;

    const std::vector<render_pipeline::offscreen_attachment> color_attachments = {
        { GL_COLOR_ATTACHMENT0, atlas_targets.atlas_texture.get_id() },
        { GL_COLOR_ATTACHMENT1, atlas_targets.wave_forcing_texture.get_id() }
    };
    if (!active_render_pipeline_->begin_offscreen_pass(atlas_targets.framebuffer, atlas_targets.width, atlas_targets.height, color_attachments))
        return;

    const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
    active_render_pipeline_->clear_offscreen_color(0, clear_color);
    active_render_pipeline_->clear_offscreen_color(1, clear_color);

    active_render_pipeline_->apply_render_state({
        false,
        GL_LESS,
        false,
        true,
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        false,
        GL_BACK
    });
    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendEquationi(0, GL_FUNC_ADD);
    glBlendFunci(1, GL_ONE, GL_ONE);
    glBlendEquationi(1, GL_FUNC_ADD);

    system.draw_planetary_water_atlas_input(
        atlas_shader,
        glm::ivec2(atlas_targets.width, atlas_targets.height));

    active_render_pipeline_->apply_render_state({});
    active_render_pipeline_->end_offscreen_pass();

    blur_planetary_water_atlas(system);
    stabilize_planetary_water_atlas(system);
}

void galactic_scene::update_planetary_tide_field(const gpu_fluid_system_component& system, int debug_mode) {
    auto* tide_shader = get_tide_field_shader();
    auto* render_resource = get_water_render_resource();
    if (!tide_shader || !tide_shader->is_vaild() || !render_resource)
        return;
    if (!active_render_pipeline_)
        return;

    auto& atlas_targets = render_resource->get_atlas_targets();
    if (atlas_targets.tide_height_texture.get_id() == 0 || atlas_targets.width <= 0 || atlas_targets.height <= 0)
        return;

    const auto& domain_textures = system.get_planetary_water_domain().get_textures();
    if (domain_textures.continuity_texture == 0 || domain_textures.water_level_texture == 0 || domain_textures.region_id_texture == 0 || domain_textures.shore_distance_texture == 0)
        return;

    const GLuint groups_x = static_cast<GLuint>((atlas_targets.width + 15) / 16);
    const GLuint groups_y = static_cast<GLuint>((atlas_targets.height + 15) / 16);
    active_render_pipeline_->dispatch_compute({
        *tide_shader,
        { groups_x, groups_y, 1u },
        {
            { 0u, GL_TEXTURE_2D, domain_textures.continuity_texture },
            { 1u, GL_TEXTURE_2D, domain_textures.water_level_texture },
            { 2u, GL_TEXTURE_2D, domain_textures.veto_texture },
            { 3u, GL_TEXTURE_2D, domain_textures.region_id_texture },
            { 4u, GL_TEXTURE_2D, domain_textures.shore_distance_texture }
        },
        {
            { 0u, atlas_targets.tide_height_texture.get_id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F }
        },
        [&](compute_shader& shader_program) {
            shader_program.set_uni_vec2("waveResolution", glm::vec2(static_cast<float>(atlas_targets.width), static_cast<float>(atlas_targets.height)));
            shader_program.set_uni_float("timeSeconds", static_cast<float>(glfwGetTime()));
            shader_program.set_uni_float("planetaryRadius", system.get_planetary_radius());
            shader_program.set_uni_float("planetaryShellThickness", system.get_planetary_shell_thickness());
            shader_program.set_uni_float("planetaryWaterSurfaceRadius", system.get_planetary_water_surface_radius());
            shader_program.set_uni_float("planetaryTidalStrength", system.get_planetary_tidal_strength());
            shader_program.set_uni_vec3("planetaryAngularVelocity", system.get_planetary_angular_velocity());
            shader_program.set_uni_int("planetaryExternalGravitySourceCount", system.get_planetary_external_gravity_source_count());
            shader_program.set_uni_vec4_array("planetaryExternalGravitySources", system.get_planetary_external_gravity_sources().data(), 8);
            shader_program.set_uni_int("waterContinuityTexture", 0);
            shader_program.set_uni_int("waterLevelTexture", 1);
            shader_program.set_uni_int("waterVetoTexture", 2);
            shader_program.set_uni_int("regionIdTexture", 3);
            shader_program.set_uni_int("shoreDistanceTexture", 4);
        }
    });

    if (debug_mode == 7)
        log_planetary_tide_texture_stats(atlas_targets.tide_height_texture.get_id(), atlas_targets.width, atlas_targets.height);
}

void galactic_scene::update_planetary_wave_field(const gpu_fluid_system_component& system, int debug_mode) {
    auto* render_resource = get_water_render_resource();
    if (!render_resource)
        return;

    const auto& atlas_targets = render_resource->get_atlas_targets();
    if (atlas_targets.width <= 0 || atlas_targets.height <= 0)
        return;

    const auto& domain = system.get_planetary_water_domain();
    const auto& domain_textures = domain.get_textures();
    if (atlas_targets.atlas_texture.get_id() == 0
        || domain_textures.physics_mask_texture == 0
        || domain_textures.region_id_texture == 0
        || domain_textures.shore_distance_texture == 0)
        return;

    planetary_wave_field_.resize_if_needed(atlas_targets.width, atlas_targets.height);
    planetary_wave_update_context update_context;
    update_context.support_atlas_texture = atlas_targets.atlas_texture.get_id();
    update_context.forcing_texture = atlas_targets.wave_forcing_texture.get_id();
    update_context.water_domain_texture = domain_textures.physics_mask_texture;
    update_context.water_level_texture = domain_textures.water_level_texture;
    update_context.tidal_height_texture = atlas_targets.tide_height_texture.get_id();
    update_context.water_veto_texture = domain_textures.veto_texture;
    update_context.region_id_texture = domain_textures.region_id_texture;
    update_context.shore_distance_texture = domain_textures.shore_distance_texture;
    update_context.dt = 1.0f / 60.0f;
    update_context.time_seconds = static_cast<float>(glfwGetTime());
    update_context.propagation_speed = 1.05f;
    update_context.forcing_scale = 0.34f;
    update_context.open_water_damping = 1.05f;
    update_context.shore_damping = 3.25f;
    update_context.shore_transition_distance = glm::max(system.get_planetary_shell_thickness() * 0.035f, 0.0016f);
    update_context.solver_forcing_scale = 0.58f;
    planetary_wave_field_.update(update_context);
    log_planetary_wave_texture_stats(
        planetary_wave_field_.get_wave_state_texture(),
        atlas_targets.atlas_texture.get_id(),
        atlas_targets.tide_height_texture.get_id(),
        domain,
        atlas_targets.width,
        atlas_targets.height,
        update_context,
        debug_mode == 7,
        runtime_resources_.planetary_wave_debug_height_scale,
        runtime_resources_.planetary_wave_debug_velocity_scale,
        runtime_resources_.planetary_wave_debug_tidal_scale);
}

void galactic_scene::apply_planetary_shell_common_uniforms(shader& target_shader, const scene_render_context& context, const gpu_fluid_system_component& system) const {
    const auto* node = system.get_node();
    if (!node)
        return;

    const auto& terrain_profile = system.get_planetary_terrain_profile();
    target_shader.set_uniform_mat4("systemModel", node->get_global_matrix_model());
    target_shader.set_uniform_mat4("view", context.camera.GetViewMatrix());
    const float aspect = context.framebuffer_height == 0 ? 1.0f : static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height);
    target_shader.set_uniform_mat4("projection", context.camera.GetProjectionMatrix(aspect));
    target_shader.set_uni_vec3("planetaryCenter", system.get_planetary_center());
    target_shader.set_uni_float("planetaryRadius", system.get_planetary_radius());
    target_shader.set_uni_float("planetaryShellThickness", system.get_planetary_shell_thickness());
    target_shader.set_uni_float("planetaryWaterSurfaceRadius", system.get_planetary_water_surface_radius());
    target_shader.set_uni_int("planetaryTerrainEnabled", system.is_planetary_terrain_enabled() ? 1 : 0);
    target_shader.set_uni_float("terrainSeaLevel", terrain_profile.sea_level);
    target_shader.set_uni_float("terrainContinentFrequency", terrain_profile.continent_frequency);
    target_shader.set_uni_float("terrainContinentWarpStrength", terrain_profile.continent_warp_strength);
    target_shader.set_uni_float("terrainLargeFrequency", terrain_profile.large_frequency);
    target_shader.set_uni_float("terrainMediumFrequency", terrain_profile.medium_frequency);
    target_shader.set_uni_float("terrainDetailFrequency", terrain_profile.detail_frequency);
    target_shader.set_uni_float("terrainRidgeFrequency", terrain_profile.ridge_frequency);
    target_shader.set_uni_float("terrainCraterStrength", terrain_profile.crater_strength);
    target_shader.set_uni_float("terrainMountainSharpness", terrain_profile.mountain_sharpness);
    target_shader.set_uni_float("terrainReliefStrength", terrain_profile.relief_strength);
    target_shader.set_uni_float("terrainDisplacementStrength", terrain_profile.displacement_strength);
    target_shader.set_uni_float("terrainContinentContrast", terrain_profile.continent_contrast);
    target_shader.set_uni_float("terrainEarthMacroContinentStrength", terrain_profile.earth_macro_continent_strength);
    target_shader.set_uni_float("terrainArchipelagoStrength", terrain_profile.archipelago_strength);
}

std::vector<render_pipeline::texture_binding> galactic_scene::build_planetary_shell_texture_bindings(const gpu_fluid_system_component& system) const {
    auto* render_resource = get_water_render_resource();
    if (!render_resource)
        return {};

    const auto& atlas_targets = render_resource->get_atlas_targets();
    const auto& domain_textures = system.get_planetary_water_domain().get_textures();
    return {
        { 0u, GL_TEXTURE_2D, atlas_targets.atlas_texture.get_id() },
        { 1u, GL_TEXTURE_2D, domain_textures.continuity_texture },
        { 2u, GL_TEXTURE_2D, system.get_planetary_water_level_texture() },
        { 3u, GL_TEXTURE_2D, planetary_wave_field_.get_render_wave_state_texture() },
        { 4u, GL_TEXTURE_2D, atlas_targets.tide_height_texture.get_id() },
        { 5u, GL_TEXTURE_2D, domain_textures.region_id_texture },
        { 6u, GL_TEXTURE_2D, domain_textures.shore_distance_texture }
    };
}

void galactic_scene::render_planetary_water_shell(const scene_render_context& context, const gpu_fluid_system_component& system) const {
    auto* shell_shader = get_water_shell_shader();
    auto* shell_mesh = get_water_shell_mesh();
    auto* render_resource = get_water_render_resource();
    if (!shell_shader || !shell_mesh || !render_resource)
        return;
    if (!active_render_pipeline_)
        return;

    const auto& atlas_targets = render_resource->get_atlas_targets();
    if (atlas_targets.atlas_texture.get_id() == 0)
        return;

    const auto* node = system.get_node();
    if (!node)
        return;

    shell_shader->use();
    apply_planetary_shell_common_uniforms(*shell_shader, context, system);
    shell_shader->set_uni_vec3("viewPos", context.camera.get_transform()->get_global_position());
    shell_shader->set_uni_vec3("lightPos", context.light_position);
    shell_shader->set_uni_vec3("lightColor", context.light_color);
    shell_shader->set_uni_float("intensity", context.light_intensity);
    shell_shader->set_uni_float("time", static_cast<float>(glfwGetTime()));
    const glm::mat4 system_model = node->get_global_matrix_model();
    const float system_scale = std::max(
        std::max(glm::length(glm::vec3(system_model[0])), glm::length(glm::vec3(system_model[1]))),
        std::max(glm::length(glm::vec3(system_model[2])), 1.0f));
    shell_shader->set_uni_vec3("planetaryCenterWorld", glm::vec3(system_model * glm::vec4(system.get_planetary_center(), 1.0f)));
    shell_shader->set_uni_float("planetarySolidRadiusWorld", system.get_planetary_radius() * system_scale);
    shell_shader->set_uni_float("planetaryShellThicknessWorld", system.get_planetary_shell_thickness() * system_scale);
    shell_shader->set_uni_float("planetDepthBiasWorld", std::max(system.get_planetary_shell_thickness() * system_scale * 0.42f, system.get_planetary_radius() * system_scale * 0.004f));
    shell_shader->set_uni_int("waveDebugMode", context.debug_mode);
    shell_shader->set_uni_int("waterAtlasTexture", 0);
    const auto& domain_textures = system.get_planetary_water_domain().get_textures();
    shell_shader->set_uni_int("waterContinuityTextureAvailable", domain_textures.continuity_texture != 0 ? 1 : 0);
    shell_shader->set_uni_int("waterContinuityTexture", 1);
    shell_shader->set_uni_int("waterLevelTextureAvailable", system.get_planetary_water_level_texture() != 0 ? 1 : 0);
    shell_shader->set_uni_int("waterLevelTexture", 2);
    shell_shader->set_uni_int("waveStateTextureAvailable", planetary_wave_field_.get_render_wave_state_texture() != 0 ? 1 : 0);
    shell_shader->set_uni_int("waveStateTexture", 3);
    shell_shader->set_uni_int("tidalHeightTextureAvailable", atlas_targets.tide_height_texture.get_id() != 0 ? 1 : 0);
    shell_shader->set_uni_int("tidalHeightTexture", 4);
    shell_shader->set_uni_int("regionIdTextureAvailable", domain_textures.region_id_texture != 0 ? 1 : 0);
    shell_shader->set_uni_int("regionIdTexture", 5);
    shell_shader->set_uni_int("shoreDistanceTextureAvailable", domain_textures.shore_distance_texture != 0 ? 1 : 0);
    shell_shader->set_uni_int("shoreDistanceTexture", 6);

    const auto texture_bindings = build_planetary_shell_texture_bindings(system);
    active_render_pipeline_->bind_textures(texture_bindings);

    active_render_pipeline_->apply_render_state({
        true,
        GL_LEQUAL,
        false,
        true,
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        true,
        GL_BACK
    });
    shell_mesh->Draw();
    active_render_pipeline_->apply_render_state({});

    active_render_pipeline_->unbind_textures(texture_bindings);
}

void galactic_scene::render_planetary_wave_debug_overlay(const scene_render_context& context, const gpu_fluid_system_component& system) const {
    auto* debug_shell_shader = get_wave_debug_shell_shader();
    auto* shell_mesh = get_water_shell_mesh();
    auto* render_resource = get_water_render_resource();
    if (context.debug_mode < 6 || context.debug_mode > 11 || !debug_shell_shader || !shell_mesh || !render_resource)
        return;
    if (!active_render_pipeline_)
        return;

    const auto& atlas_targets = render_resource->get_atlas_targets();
    if (atlas_targets.atlas_texture.get_id() == 0)
        return;

    const auto& domain_textures = system.get_planetary_water_domain().get_textures();
    const auto* node = system.get_node();
    if (!node)
        return;

    debug_shell_shader->use();
    apply_planetary_shell_common_uniforms(*debug_shell_shader, context, system);
    debug_shell_shader->set_uni_int("waveDebugMode", context.debug_mode);
    debug_shell_shader->set_uni_int("waterAtlasTexture", 0);
    debug_shell_shader->set_uni_int("waterContinuityTextureAvailable", domain_textures.continuity_texture != 0 ? 1 : 0);
    debug_shell_shader->set_uni_int("waterContinuityTexture", 1);
    debug_shell_shader->set_uni_int("waterLevelTextureAvailable", system.get_planetary_water_level_texture() != 0 ? 1 : 0);
    debug_shell_shader->set_uni_int("waterLevelTexture", 2);
    debug_shell_shader->set_uni_int("waveStateTextureAvailable", planetary_wave_field_.get_render_wave_state_texture() != 0 ? 1 : 0);
    debug_shell_shader->set_uni_int("waveStateTexture", 3);
    debug_shell_shader->set_uni_int("tidalHeightTextureAvailable", atlas_targets.tide_height_texture.get_id() != 0 ? 1 : 0);
    debug_shell_shader->set_uni_int("tidalHeightTexture", 4);
    debug_shell_shader->set_uni_int("regionIdTextureAvailable", domain_textures.region_id_texture != 0 ? 1 : 0);
    debug_shell_shader->set_uni_int("regionIdTexture", 5);
    debug_shell_shader->set_uni_int("shoreDistanceTextureAvailable", domain_textures.shore_distance_texture != 0 ? 1 : 0);
    debug_shell_shader->set_uni_int("shoreDistanceTexture", 6);
    debug_shell_shader->set_uni_float("debugWaveHeightScale", runtime_resources_.planetary_wave_debug_height_scale);
    debug_shell_shader->set_uni_float("debugWaveVelocityScale", runtime_resources_.planetary_wave_debug_velocity_scale);
    debug_shell_shader->set_uni_float("debugTidalScale", runtime_resources_.planetary_wave_debug_tidal_scale);

    const auto texture_bindings = build_planetary_shell_texture_bindings(system);
    active_render_pipeline_->bind_textures(texture_bindings);

    active_render_pipeline_->apply_render_state({
        true,
        GL_LEQUAL,
        false,
        true,
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        true,
        GL_BACK
    });
    shell_mesh->Draw();
    active_render_pipeline_->apply_render_state({});

    active_render_pipeline_->unbind_textures(texture_bindings);
}

bool galactic_scene::render_runtime(engine& engine, const scene_render_context& context) {
    (void)engine;
    active_render_pipeline_ = &context.render_pipeline;

    const bool needs_particle_surface_pass = std::ranges::any_of(get_gpu_fluid_systems(), [](const auto* system) {
        return system && system->supports_particle_surface_pass();
    });

    if (!needs_particle_surface_pass)
        return false;

    ensure_planetary_water_atlas_targets(planetary_water_atlas_default_width, planetary_water_atlas_default_height);
    return false;
}

bool galactic_scene::render_fluid_system(engine& engine, const scene_render_context& context, const gpu_fluid_system_component& system) {
    (void)engine;
    active_render_pipeline_ = &context.render_pipeline;

    if (!system.supports_particle_surface_pass())
        return false;

    if (system.get_debug_visualization_mode() != fluid_debug_visualization_mode::none) {
        auto debug_draw_section = frame_profiler::measure_active("render_fluid_system_debug_draw");
        system.draw(&context.camera, system.requires_scene_depth_texture() ? context.render_pipeline.get_scene_depth_texture_id() : 0);
        return true;
    }

    {
        auto ensure_targets_section = frame_profiler::measure_active("render_fluid_system_ensure_atlas_targets");
        ensure_planetary_water_atlas_targets(planetary_water_atlas_default_width, planetary_water_atlas_default_height);
    }
    {
        auto atlas_input_section = frame_profiler::measure_active("render_fluid_system_atlas_input");
        render_planetary_water_atlas_input(system);
    }
    {
        auto tide_field_section = frame_profiler::measure_active("render_fluid_system_tide_field");
        update_planetary_tide_field(system, context.debug_mode);
    }
    {
        auto wave_field_section = frame_profiler::measure_active("render_fluid_system_wave_field");
        update_planetary_wave_field(system, context.debug_mode);
    }
    {
        auto water_shell_section = frame_profiler::measure_active("render_fluid_system_water_shell");
        render_planetary_water_shell(context, system);
    }
    if (context.debug_mode >= 6 && context.debug_mode <= 11) {
        auto wave_debug_overlay_section = frame_profiler::measure_active("render_fluid_system_wave_debug_overlay");
        render_planetary_wave_debug_overlay(context, system);
    }
    return true;
}

void galactic_scene::update() {
    if (camera_node_) {
        const glm::vec3 camera_position = camera_node_->get_global_position();
        if (background_star_node_)
            background_star_node_->set_global_position(camera_position);
        if (background_galaxy_node_)
            background_galaxy_node_->set_global_position(camera_position);
    }

    scene::update();

    constexpr float fixed_dt = 1.f / 60.f;
    const float rotation_step = fixed_dt * get_simulation_speed() * axial_rotation_base_speed;
    const size_t count = std::min(std::size(planet_spin_node_names), std::size(planet_spin_speed_multipliers));
    for (size_t i = 0; i < count; ++i) {
        auto* node = find_scene_node(planet_spin_node_names[i]);
        if (!node)
            continue;

        glm::vec3 rotation = node->get_rotation();
        rotation.y += rotation_step * planet_spin_speed_multipliers[i];
        node->set_rotation(rotation);
    }
}

void galactic_scene::handle_input(engine& engine, float dt) {
    (void)engine;
    (void)dt;

    const bool prev_down = input_system::is_key_down(GLFW_KEY_H);
    if (prev_down && !previous_wave_debug_prev_down_) {
        wave_debug_mode_ = (wave_debug_mode_ + 12 - 1) % 12;
        std::cout << "[galactic_wave_debug_mode] " << wave_debug_mode_ << std::endl;
    }
    previous_wave_debug_prev_down_ = prev_down;

    const bool next_down = input_system::is_key_down(GLFW_KEY_J);
    if (next_down && !previous_wave_debug_next_down_) {
        wave_debug_mode_ = (wave_debug_mode_ + 1) % 12;
        std::cout << "[galactic_wave_debug_mode] " << wave_debug_mode_ << std::endl;
    }
    previous_wave_debug_next_down_ = next_down;
}

