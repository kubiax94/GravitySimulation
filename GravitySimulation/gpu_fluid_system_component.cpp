#include "gpu_fluid_system_component.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Camera.h"
#include "frame_profiler.h"
#include "Mesh.h"
#include "rigid_body.h"
#include "Scene.h"
#include "Shader.h"

namespace {
constexpr unsigned int adaptive_budget_recovery_threshold = 8u;
constexpr size_t planetary_flood_guidance_point_count = 64u;
constexpr size_t planetary_flood_guidance_sample_count = 16384u;
constexpr float planetary_flood_active_threshold = 0.68f;
constexpr float planetary_respawn_depth_threshold = 0.28f;

float build_aspect_ratio() {
    int fbw = 1280;
    int fbh = 720;
    if (GLFWwindow* ctx = glfwGetCurrentContext())
        glfwGetFramebufferSize(ctx, &fbw, &fbh);

    return fbh == 0 ? 1.f : static_cast<float>(fbw) / static_cast<float>(fbh);
}

float build_framebuffer_height() {
    int fbw = 1280;
    int fbh = 720;
    if (GLFWwindow* ctx = glfwGetCurrentContext())
        glfwGetFramebufferSize(ctx, &fbw, &fbh);

    return static_cast<float>(std::max(fbh, 1));
}

GLuint compute_grid_axis(float extent, float cell_size) {
    const float safe_cell_size = glm::max(cell_size, 0.0001f);
    return static_cast<GLuint>(glm::max(1.0f, glm::ceil(extent / safe_cell_size)));
}

float safe_sample_step(size_t count, size_t samples) {
    return static_cast<float>(std::max<size_t>(1u, count / std::max<size_t>(samples, 1u)));
}

void report_planetary_hydrology_debug(
    const planet_terrain::ocean_basin_graph& basin_graph,
    const planet_terrain::ocean_flood_state& flood_state,
    const std::vector<unsigned char>& flood_mask_data,
    float target_coverage) {
    const size_t total_samples = basin_graph.samples.size();
    size_t flooded_sample_count = 0u;
    std::vector<size_t> region_sizes;
    region_sizes.reserve(flood_state.regions.size());
    std::vector<float> spill_radii;
    spill_radii.reserve(total_samples);
    for (const auto& sample : basin_graph.samples) {
        spill_radii.push_back(sample.spill_radius);
    }

    for (const int region_index : flood_state.sample_region_indices) {
        if (region_index >= 0)
            ++flooded_sample_count;
    }

    for (const auto& region : flood_state.regions)
        region_sizes.push_back(region.sample_indices.size());
    std::sort(region_sizes.begin(), region_sizes.end(), std::greater<size_t>());

    std::sort(spill_radii.begin(), spill_radii.end());
    const auto spill_at = [&](float t) {
        if (spill_radii.empty())
            return 0.0f;
        const size_t index = std::min(
            spill_radii.size() - 1u,
            static_cast<size_t>(t * static_cast<float>(spill_radii.size() - 1u)));
        return spill_radii[index];
    };

    const size_t filled_texel_count = static_cast<size_t>(std::count_if(
        flood_mask_data.begin(),
        flood_mask_data.end(),
        [](unsigned char value) { return value > 0u; }));

    std::ostringstream stream;
    stream << "[planetary_hydrology_debug]"
        << " target_coverage=" << target_coverage
        << " water_surface_radius=" << flood_state.water_surface_radius
        << " minima=" << basin_graph.minima_sample_indices.size()
        << " basins=" << basin_graph.basins.size()
        << " flooded_regions=" << flood_state.regions.size()
        << " flooded_samples=" << flooded_sample_count << "/" << total_samples
        << " (" << (total_samples > 0u ? static_cast<double>(flooded_sample_count) / static_cast<double>(total_samples) : 0.0) << ")"
        << " mask_texels=" << filled_texel_count << "/" << flood_mask_data.size()
        << " (" << (!flood_mask_data.empty() ? static_cast<double>(filled_texel_count) / static_cast<double>(flood_mask_data.size()) : 0.0) << ")"
        << " spill[p10=" << spill_at(0.10f)
        << ", p50=" << spill_at(0.50f)
        << ", p90=" << spill_at(0.90f)
        << "]";

    if (!region_sizes.empty()) {
        stream << " top_regions=";
        for (size_t i = 0; i < std::min<size_t>(3u, region_sizes.size()); ++i) {
            if (i > 0u)
                stream << ",";
            stream << region_sizes[i];
        }
    }

    std::cout << stream.str() << std::endl;
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
    release_planetary_flood_mask_texture();
    release_render_flood_mask_gpu_query();
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
    compute_shader_->add_ssbo(respawn_candidate_count_binding_, std::vector<unsigned int>(1u, 0u));
    compute_shader_->add_ssbo(respawn_candidate_indices_binding_, std::vector<unsigned int>(max_respawn_candidate_count_, 0u));
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

void gpu_fluid_system_component::release_planetary_flood_mask_texture() {
    if (planetary_physics_flood_mask_texture_ != 0) {
        glDeleteTextures(1, &planetary_physics_flood_mask_texture_);
        planetary_physics_flood_mask_texture_ = 0;
    }

    if (planetary_flood_mask_texture_ != 0) {
        glDeleteTextures(1, &planetary_flood_mask_texture_);
        planetary_flood_mask_texture_ = 0;
    }

    if (planetary_water_level_texture_ != 0) {
        glDeleteTextures(1, &planetary_water_level_texture_);
        planetary_water_level_texture_ = 0;
    }
}

void gpu_fluid_system_component::release_render_flood_mask_gpu_query() const {
    if (render_flood_mask_gpu_query_ != 0) {
        glDeleteQueries(1, &render_flood_mask_gpu_query_);
        render_flood_mask_gpu_query_ = 0;
    }

    render_flood_mask_gpu_query_pending_ = false;
}

void gpu_fluid_system_component::update_adaptive_budget() {
    const unsigned int target_substeps = std::max(1u, solver_substeps_);
    const unsigned int target_iterations = std::max(1u, constraint_iterations_);

    runtime_solver_substeps_ = std::min(runtime_solver_substeps_, target_substeps);
    runtime_constraint_iterations_ = std::min(runtime_constraint_iterations_, target_iterations);

    if (!gpu_completion_fence_)
        return;

    const double sync_start = glfwGetTime();
    const GLenum status = glClientWaitSync(gpu_completion_fence_, 0, 0);
    gpu_completion_fence_wait_timing_.add_sample((glfwGetTime() - sync_start) * 1000.0);
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
    planetary_shell_thickness_ = planet_terrain::resolved_ocean_shell_thickness(shell_thickness, particle_radius_);
    planetary_gravity_strength_ = glm::max(gravity_strength, 0.0f);

    const float required_extent = planetary_radius_ + planetary_shell_thickness_ + glm::max(interaction_radius_, particle_radius_ * 2.0f);
    bounds_.min = glm::min(bounds_.min, planetary_center_ - glm::vec3(required_extent));
    bounds_.max = glm::max(bounds_.max, planetary_center_ + glm::vec3(required_extent));
    rebuild_grid_metadata();
    if (initialized_)
        rebuild_grid_buffers();

    rebuild_planetary_water_surface_radius();
    rebuild_planetary_flood_guidance();
}

void gpu_fluid_system_component::set_planetary_flow_tuning(float downslope_strength, float floor_attraction_strength, float wetting_strength, float bottom_friction, float bottom_normal_damping) {
    planetary_downslope_strength_ = glm::max(downslope_strength, 0.0f);
    planetary_floor_attraction_strength_ = glm::max(floor_attraction_strength, 0.0f);
    planetary_flood_guidance_strength_ = glm::max(wetting_strength, 0.0f);
    planetary_bottom_friction_ = glm::clamp(bottom_friction, 0.0f, 1.0f);
    planetary_bottom_normal_damping_ = glm::clamp(bottom_normal_damping, 0.0f, 1.0f);
}

void gpu_fluid_system_component::set_planetary_flood_guidance_strength(float strength) {
    planetary_flood_guidance_strength_ = glm::max(strength, 0.0f);
}

void gpu_fluid_system_component::set_planetary_surface_layer_tuning(float thickness_scale, float attraction_strength, float normal_velocity_damping) {
    planetary_surface_layer_thickness_scale_ = glm::clamp(thickness_scale, 0.1f, 1.0f);
    planetary_surface_layer_attraction_strength_ = glm::max(attraction_strength, 0.0f);
    planetary_surface_layer_normal_velocity_damping_ = glm::clamp(normal_velocity_damping, 0.0f, 1.0f);
}

void gpu_fluid_system_component::set_planetary_rotation_tuning(float rotation_rate_scale, float coriolis_strength, float tidal_strength) {
    planetary_rotation_rate_scale_ = glm::max(rotation_rate_scale, 0.0f);
    planetary_coriolis_strength_ = glm::max(coriolis_strength, 0.0f);
    planetary_tidal_strength_ = glm::max(tidal_strength, 0.0f);
}

void gpu_fluid_system_component::set_planetary_respawn_management(bool enabled, unsigned int interval_frames) {
    planetary_respawn_management_enabled_ = enabled;
    planetary_respawn_interval_frames_ = std::max(1u, interval_frames);
    planetary_respawn_frame_counter_ = 0u;
    planetary_respawn_scan_cursor_ = 0u;
}

void gpu_fluid_system_component::set_planetary_water_coverage(float coverage) {
    planetary_water_coverage_ = glm::clamp(coverage, 0.0f, 1.0f);
    rebuild_planetary_water_surface_radius();
    rebuild_planetary_flood_guidance();
}

void gpu_fluid_system_component::set_planetary_surface_frame_node(scene_node* frame_node) {
    planetary_surface_frame_node_ = frame_node;
}

void gpu_fluid_system_component::set_planetary_terrain_profile(const planet_terrain::rocky_planet_profile& profile) {
    planetary_terrain_enabled_ = true;
    planetary_terrain_profile_ = profile;
    if (planetary_water_coverage_ <= 0.0f)
        planetary_water_coverage_ = glm::clamp(profile.ocean_coverage, 0.0f, 1.0f);
    rebuild_planetary_water_surface_radius();
    rebuild_planetary_flood_guidance();
}

void gpu_fluid_system_component::rebuild_planetary_water_surface_radius() {
    planetary_water_surface_radius_ = planetary_radius_ + planetary_shell_thickness_;
    if (!planetary_surface_enabled_ || !planetary_terrain_enabled_ || planetary_water_coverage_ <= 0.0f)
        return;

    planetary_water_surface_radius_ = glm::clamp(
        planet_terrain::estimate_water_surface_radius(planetary_radius_, planetary_shell_thickness_, particle_radius_, planetary_water_coverage_, planetary_terrain_profile_),
        planetary_radius_ + particle_radius_ * 0.5f,
        planetary_radius_ + planetary_shell_thickness_);
}

void gpu_fluid_system_component::rebuild_planetary_flood_guidance() {
    planetary_flood_respawn_normals_.clear();
    planetary_flood_respawn_radii_.clear();
    planetary_physics_flood_mask_data_.clear();
    planetary_flood_mask_data_.clear();
    planetary_water_level_data_.clear();
    release_planetary_flood_mask_texture();
    planetary_respawn_cursor_ = 0u;
    planetary_respawn_scan_cursor_ = 0u;

    if (!planetary_surface_enabled_ || !planetary_terrain_enabled_ || planetary_water_coverage_ <= 0.0f)
        return;

    planet_terrain::ocean_seed_generation_params params;
    params.target_particle_count = particle_count_;
    params.base_radius = planetary_radius_;
    params.shell_thickness = planetary_shell_thickness_;
    params.particle_radius = particle_radius_;
    params.coverage = planetary_water_coverage_;
    params.candidate_count = planetary_flood_guidance_sample_count;
    params.primary_regions_only = true;

    const auto ocean_seed_data = planet_terrain::generate_ocean_seed_data(params, planetary_terrain_profile_);
    const auto& basin_graph = ocean_seed_data.basin_graph;
    const auto& flood_state = ocean_seed_data.flood_state;

    if (basin_graph.samples.empty())
        return;

    bool added_deep_respawn_samples = false;
    for (const auto& region : flood_state.regions) {
        for (const size_t sample_index : region.sample_indices) {
            const auto& sample = basin_graph.samples[sample_index];
            const float depth01 = glm::clamp(
                (region.water_surface_radius - sample.floor_radius) / glm::max(planetary_shell_thickness_, 0.0001f),
                0.0f,
                1.0f);
            if (depth01 < planetary_respawn_depth_threshold)
                continue;

            const size_t repeat_count = 1u + static_cast<size_t>(glm::floor((depth01 - planetary_respawn_depth_threshold) * 7.0f));
            const float respawn_fill = glm::clamp(0.30f + depth01 * 0.30f, 0.24f, 0.68f);
            const float respawn_radius = glm::mix(sample.floor_radius, region.water_surface_radius, respawn_fill);
            for (size_t repeat = 0; repeat < repeat_count; ++repeat) {
                planetary_flood_respawn_normals_.push_back(sample.normal);
                planetary_flood_respawn_radii_.push_back(respawn_radius);
            }
            added_deep_respawn_samples = true;
        }
    }

    if (!added_deep_respawn_samples) {
        for (const auto& region : flood_state.regions) {
            for (const size_t sample_index : region.sample_indices) {
                const auto& sample = basin_graph.samples[sample_index];
                const float depth01 = glm::clamp(
                    (region.water_surface_radius - sample.floor_radius) / glm::max(planetary_shell_thickness_, 0.0001f),
                    0.0f,
                    1.0f);
                const size_t repeat_count = 1u + static_cast<size_t>(glm::floor(depth01 * 4.0f));
                const float respawn_fill = glm::clamp(0.24f + depth01 * 0.24f, 0.20f, 0.58f);
                const float respawn_radius = glm::mix(sample.floor_radius, region.water_surface_radius, respawn_fill);
                for (size_t repeat = 0; repeat < repeat_count; ++repeat) {
                    planetary_flood_respawn_normals_.push_back(sample.normal);
                    planetary_flood_respawn_radii_.push_back(respawn_radius);
                }
            }
        }
    }

    rebuild_planetary_flood_mask_texture(basin_graph, flood_state);
    report_planetary_hydrology_debug(basin_graph, flood_state, planetary_flood_mask_data_, planetary_water_coverage_);
}

void gpu_fluid_system_component::rebuild_planetary_flood_mask_texture(const planet_terrain::ocean_basin_graph& basin_graph, const planet_terrain::ocean_flood_state& flood_state) {
    if (basin_graph.samples.empty() || flood_state.sample_region_indices.empty())
        return;

    planetary_physics_flood_mask_data_.assign(static_cast<size_t>(planetary_flood_mask_texture_width_) * static_cast<size_t>(planetary_flood_mask_texture_height_), 0u);
    planetary_flood_mask_data_.assign(static_cast<size_t>(planetary_flood_mask_texture_width_) * static_cast<size_t>(planetary_flood_mask_texture_height_), 0u);
    planetary_water_level_data_.assign(static_cast<size_t>(planetary_flood_mask_texture_width_) * static_cast<size_t>(planetary_flood_mask_texture_height_), 0.0f);
    std::vector<float> physics_mask(planetary_flood_mask_data_.size(), 0.0f);
    std::vector<float> coverage_mask(planetary_flood_mask_data_.size(), 0.0f);
    std::vector<float> next_mask(planetary_flood_mask_data_.size(), 0.0f);
    std::vector<float> next_physics_mask(planetary_flood_mask_data_.size(), 0.0f);
    std::vector<float> water_level_accum(planetary_flood_mask_data_.size(), 0.0f);

    for (size_t sample_index = 0; sample_index < basin_graph.samples.size(); ++sample_index) {
        const int region_index = flood_state.sample_region_indices[sample_index];
        if (region_index < 0)
            continue;

        const auto& sample = basin_graph.samples[sample_index];
        const glm::vec3& normal = sample.normal;
        const float latitude = std::asin(glm::clamp(normal.y, -1.0f, 1.0f));
        const float longitude = std::atan2(normal.z, normal.x);
        const float u = (longitude + glm::pi<float>()) / glm::two_pi<float>();
        const float v = (latitude + glm::half_pi<float>()) / glm::pi<float>();
        const int center_x = glm::clamp(static_cast<int>(u * static_cast<float>(planetary_flood_mask_texture_width_)), 0, planetary_flood_mask_texture_width_ - 1);
        const int center_y = glm::clamp(static_cast<int>(v * static_cast<float>(planetary_flood_mask_texture_height_)), 0, planetary_flood_mask_texture_height_ - 1);
        const float water_surface_radius = flood_state.regions[static_cast<size_t>(region_index)].water_surface_radius;
        const float normalized_water_level = glm::clamp(
            (water_surface_radius - planetary_radius_) / glm::max(planetary_shell_thickness_, 0.0001f),
            0.0f,
            1.0f);
        const float depth01 = glm::clamp(
            (water_surface_radius - sample.floor_radius) / glm::max(planetary_shell_thickness_, 0.0001f),
            0.0f,
            1.0f);
        const float latitude_cos = glm::max(std::cos(latitude), 0.35f);
        const float longitude_scale = glm::clamp(1.0f / latitude_cos, 1.0f, 2.25f);
        const float physics_texel_radius_x = glm::mix(0.9f, 1.8f, depth01) * longitude_scale;
        const float physics_texel_radius_y = glm::mix(0.9f, 1.6f, depth01);
        const float texel_radius_x = glm::mix(1.75f, 4.6f, depth01) * longitude_scale;
        const float texel_radius_y = glm::mix(1.75f, 4.2f, depth01);
        const int physics_radius_x = std::max(1, static_cast<int>(glm::ceil(physics_texel_radius_x)));
        const int physics_radius_y = std::max(1, static_cast<int>(glm::ceil(physics_texel_radius_y)));
        const int radius_x = std::max(1, static_cast<int>(glm::ceil(texel_radius_x)));
        const int radius_y = std::max(1, static_cast<int>(glm::ceil(texel_radius_y)));
        const float physics_strength = glm::mix(0.78f, 1.0f, depth01);
        const float sample_strength = glm::mix(0.42f, 0.96f, depth01);

        for (int offset_y = -physics_radius_y; offset_y <= physics_radius_y; ++offset_y) {
            const int y = center_y + offset_y;
            if (y < 0 || y >= planetary_flood_mask_texture_height_)
                continue;

            for (int offset_x = -physics_radius_x; offset_x <= physics_radius_x; ++offset_x) {
                const float normalized_offset_x = static_cast<float>(offset_x) / physics_texel_radius_x;
                const float normalized_offset_y = static_cast<float>(offset_y) / physics_texel_radius_y;
                const float distance_sq = normalized_offset_x * normalized_offset_x + normalized_offset_y * normalized_offset_y;
                if (distance_sq > 1.0f)
                    continue;

                const int wrapped_x = (center_x + offset_x + planetary_flood_mask_texture_width_) % planetary_flood_mask_texture_width_;
                const float falloff = 1.0f - distance_sq;
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(planetary_flood_mask_texture_width_) + static_cast<size_t>(wrapped_x);
                physics_mask[pixel_index] = glm::clamp(std::max(physics_mask[pixel_index], physics_strength * falloff), 0.0f, 1.0f);
                water_level_accum[pixel_index] = std::max(water_level_accum[pixel_index], normalized_water_level);
            }
        }

        for (int offset_y = -radius_y; offset_y <= radius_y; ++offset_y) {
            const int y = center_y + offset_y;
            if (y < 0 || y >= planetary_flood_mask_texture_height_)
                continue;

            for (int offset_x = -radius_x; offset_x <= radius_x; ++offset_x) {
                const float normalized_offset_x = static_cast<float>(offset_x) / texel_radius_x;
                const float normalized_offset_y = static_cast<float>(offset_y) / texel_radius_y;
                const float distance_sq = normalized_offset_x * normalized_offset_x + normalized_offset_y * normalized_offset_y;
                if (distance_sq > 1.0f)
                    continue;

                const int wrapped_x = (center_x + offset_x + planetary_flood_mask_texture_width_) % planetary_flood_mask_texture_width_;
                const float falloff = 1.0f - distance_sq;
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(planetary_flood_mask_texture_width_) + static_cast<size_t>(wrapped_x);
                coverage_mask[pixel_index] = glm::clamp(
                    coverage_mask[pixel_index] + sample_strength * falloff,
                    0.0f,
                    1.0f);
            }
        }
    }

    auto sample_mask = [&](const std::vector<float>& mask, int x, int y) -> float {
        x = (x + planetary_flood_mask_texture_width_) % planetary_flood_mask_texture_width_;
        y = glm::clamp(y, 0, planetary_flood_mask_texture_height_ - 1);
        return mask[static_cast<size_t>(y) * static_cast<size_t>(planetary_flood_mask_texture_width_) + static_cast<size_t>(x)];
    };

    for (int pass = 0; pass < 1; ++pass) {
        for (int y = 0; y < planetary_flood_mask_texture_height_; ++y) {
            for (int x = 0; x < planetary_flood_mask_texture_width_; ++x) {
                float neighbor_sum = 0.0f;
                float weight_total = 0.0f;
                float neighbor_max = 0.0f;
                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        const float kernel = (offset_x == 0 && offset_y == 0) ? 0.34f : 0.0825f;
                        const float value = sample_mask(physics_mask, x + offset_x, y + offset_y);
                        neighbor_sum += value * kernel;
                        weight_total += kernel;
                        neighbor_max = std::max(neighbor_max, value);
                    }
                }

                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(planetary_flood_mask_texture_width_) + static_cast<size_t>(x);
                const float center = physics_mask[pixel_index];
                const float averaged = weight_total > 0.0f ? neighbor_sum / weight_total : center;
                next_physics_mask[pixel_index] = glm::clamp(std::max(center, std::max(averaged * 0.92f, neighbor_max * 0.72f)), 0.0f, 1.0f);
            }
        }

        physics_mask.swap(next_physics_mask);
    }

    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < planetary_flood_mask_texture_height_; ++y) {
            for (int x = 0; x < planetary_flood_mask_texture_width_; ++x) {
                float weighted_sum = 0.0f;
                float weight_total = 0.0f;
                float max_neighbor = 0.0f;
                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        const float kernel = (offset_x == 0 && offset_y == 0)
                            ? 0.24f
                            : ((offset_x == 0 || offset_y == 0) ? 0.12f : 0.07f);
                        const float value = sample_mask(coverage_mask, x + offset_x, y + offset_y);
                        weighted_sum += value * kernel;
                        weight_total += kernel;
                        max_neighbor = std::max(max_neighbor, value);
                    }
                }

                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(planetary_flood_mask_texture_width_) + static_cast<size_t>(x);
                const float center = coverage_mask[pixel_index];
                const float smoothed = weight_total > 0.0f ? weighted_sum / weight_total : center;
                const float stitched = std::max(center, max_neighbor * 0.82f);
                next_mask[pixel_index] = glm::clamp(std::max(stitched, smoothed * 0.96f), 0.0f, 1.0f);
            }
        }

        coverage_mask.swap(next_mask);
    }

    for (size_t pixel_index = 0; pixel_index < coverage_mask.size(); ++pixel_index) {
        const float physics_value = glm::smoothstep(0.42f, 0.74f, physics_mask[pixel_index]);
        planetary_physics_flood_mask_data_[pixel_index] = static_cast<unsigned char>(glm::clamp(physics_value, 0.0f, 1.0f) * 255.0f);
        const float value = glm::smoothstep(0.32f, 0.72f, coverage_mask[pixel_index]);
        planetary_flood_mask_data_[pixel_index] = static_cast<unsigned char>(glm::clamp(value, 0.0f, 1.0f) * 255.0f);
        planetary_water_level_data_[pixel_index] = physics_value > 0.01f ? water_level_accum[pixel_index] : 0.0f;
    }

    glGenTextures(1, &planetary_physics_flood_mask_texture_);
    if (planetary_physics_flood_mask_texture_ != 0) {
        glBindTexture(GL_TEXTURE_2D, planetary_physics_flood_mask_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R8,
            planetary_flood_mask_texture_width_,
            planetary_flood_mask_texture_height_,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            planetary_physics_flood_mask_data_.data());
    }

    glGenTextures(1, &planetary_flood_mask_texture_);
    if (planetary_flood_mask_texture_ == 0)
        return;

    glBindTexture(GL_TEXTURE_2D, planetary_flood_mask_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R8,
        planetary_flood_mask_texture_width_,
        planetary_flood_mask_texture_height_,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        planetary_flood_mask_data_.data());

    glGenTextures(1, &planetary_water_level_texture_);
    if (planetary_water_level_texture_ != 0) {
        glBindTexture(GL_TEXTURE_2D, planetary_water_level_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R16F,
            planetary_flood_mask_texture_width_,
            planetary_flood_mask_texture_height_,
            0,
            GL_RED,
            GL_FLOAT,
            planetary_water_level_data_.data());
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

float gpu_fluid_system_component::sample_planetary_flood_mask(const glm::vec3& surface_normal) const {
    if (planetary_physics_flood_mask_data_.empty())
        return -1.0f;

    const glm::vec3 safe_normal = glm::dot(surface_normal, surface_normal) > 0.000001f
        ? glm::normalize(surface_normal)
        : glm::vec3(0.f, 1.f, 0.f);
    const float latitude = std::asin(glm::clamp(safe_normal.y, -1.0f, 1.0f));
    const float longitude = std::atan2(safe_normal.z, safe_normal.x);
    const float u = (longitude + glm::pi<float>()) / glm::two_pi<float>();
    const float v = (latitude + glm::half_pi<float>()) / glm::pi<float>();
    const int x = glm::clamp(static_cast<int>(u * static_cast<float>(planetary_flood_mask_texture_width_)), 0, planetary_flood_mask_texture_width_ - 1);
    const int y = glm::clamp(static_cast<int>(v * static_cast<float>(planetary_flood_mask_texture_height_)), 0, planetary_flood_mask_texture_height_ - 1);
    const float value = static_cast<float>(planetary_physics_flood_mask_data_[static_cast<size_t>(y) * static_cast<size_t>(planetary_flood_mask_texture_width_) + static_cast<size_t>(x)]) / 255.0f;
    return glm::smoothstep(planetary_flood_active_threshold, 0.92f, value);
}

float gpu_fluid_system_component::sample_planetary_water_level(const glm::vec3& surface_normal) const {
    if (planetary_water_level_data_.empty())
        return -1.0f;

    const glm::vec3 safe_normal = glm::dot(surface_normal, surface_normal) > 0.000001f
        ? glm::normalize(surface_normal)
        : glm::vec3(0.f, 1.f, 0.f);
    const float latitude = std::asin(glm::clamp(safe_normal.y, -1.0f, 1.0f));
    const float longitude = std::atan2(safe_normal.z, safe_normal.x);
    const float u = (longitude + glm::pi<float>()) / glm::two_pi<float>();
    const float v = (latitude + glm::half_pi<float>()) / glm::pi<float>();
    const int x = glm::clamp(static_cast<int>(u * static_cast<float>(planetary_flood_mask_texture_width_)), 0, planetary_flood_mask_texture_width_ - 1);
    const int y = glm::clamp(static_cast<int>(v * static_cast<float>(planetary_flood_mask_texture_height_)), 0, planetary_flood_mask_texture_height_ - 1);
    return planetary_water_level_data_[static_cast<size_t>(y) * static_cast<size_t>(planetary_flood_mask_texture_width_) + static_cast<size_t>(x)];
}

void gpu_fluid_system_component::update_planetary_particle_respawn(const glm::mat3& planetary_surface_from_simulation) {
    if (!planetary_respawn_management_enabled_
        || !planetary_terrain_enabled_
        || !compute_shader_
        || planetary_flood_respawn_normals_.empty()
        || planetary_flood_respawn_radii_.empty())
        return;

    const double respawn_start = glfwGetTime();

    ++planetary_respawn_frame_counter_;
    if (planetary_respawn_frame_counter_ < planetary_respawn_interval_frames_)
        return;

    planetary_respawn_frame_counter_ = 0u;

    std::vector<unsigned int> respawn_candidate_count_data;
    std::vector<unsigned int> respawn_candidate_indices;
    {
        auto section = frame_profiler::measure_active("fixed_update_fluid_respawn_readback");
        const double readback_start = glfwGetTime();
        compute_shader_->get_binding_data<unsigned int>(respawn_candidate_count_binding_, respawn_candidate_count_data);
        if (!respawn_candidate_count_data.empty() && respawn_candidate_count_data[0] > 0u) {
            const unsigned int available_count = std::min<unsigned int>(respawn_candidate_count_data[0], static_cast<unsigned int>(max_respawn_candidate_count_));
            std::vector<size_t> candidate_index_readback_indices(available_count);
            std::iota(candidate_index_readback_indices.begin(), candidate_index_readback_indices.end(), size_t{ 0u });
            compute_shader_->get_binding_data_indices<unsigned int>(respawn_candidate_indices_binding_, candidate_index_readback_indices, respawn_candidate_indices);
        }
        respawn_ssbo_readback_timing_.add_sample((glfwGetTime() - readback_start) * 1000.0);
    }

    if (respawn_candidate_indices.empty()) {
        respawn_total_timing_.add_sample((glfwGetTime() - respawn_start) * 1000.0);
        return;
    }

    std::sort(respawn_candidate_indices.begin(), respawn_candidate_indices.end());
    respawn_candidate_indices.erase(std::unique(respawn_candidate_indices.begin(), respawn_candidate_indices.end()), respawn_candidate_indices.end());

    std::vector<size_t> respawn_indices(respawn_candidate_indices.begin(), respawn_candidate_indices.end());
    std::vector<fluid_particle> particles;
    {
        auto section = frame_profiler::measure_active("fixed_update_fluid_respawn_readback_particles");
        const double particle_readback_start = glfwGetTime();
        compute_shader_->get_binding_data_indices<fluid_particle>(particle_binding_, respawn_indices, particles);
        respawn_ssbo_readback_timing_.add_sample((glfwGetTime() - particle_readback_start) * 1000.0);
    }

    if (particles.empty()) {
        respawn_total_timing_.add_sample((glfwGetTime() - respawn_start) * 1000.0);
        return;
    }

    const glm::mat3 simulation_from_surface = glm::transpose(planetary_surface_from_simulation);
    bool changed = false;
    {
        const double classification_start = glfwGetTime();
        const size_t particle_count = particles.size();
        for (size_t processed_count = 0; processed_count < particle_count; ++processed_count) {
            fluid_particle& particle = particles[processed_count];
            glm::vec3 position = particle.position;
            glm::vec3 radial = position - planetary_center_;
            if (glm::dot(radial, radial) <= 0.000001f)
                continue;

            glm::vec3 simulation_normal = glm::normalize(radial);
            glm::vec3 surface_normal = planetary_surface_from_simulation * simulation_normal;
            surface_normal = glm::normalize(surface_normal);

            const float physics_flood_mask = sample_planetary_flood_mask(surface_normal);
            const float water_level = sample_planetary_water_level(surface_normal);
            if (physics_flood_mask >= 0.5f && water_level > planetary_respawn_depth_threshold)
                continue;

            const size_t respawn_slot = planetary_respawn_cursor_ % planetary_flood_respawn_normals_.size();
            const glm::vec3 respawn_surface_normal = planetary_flood_respawn_normals_[respawn_slot];
            const float respawn_radius = glm::clamp(
                planetary_flood_respawn_radii_[respawn_slot],
                planetary_radius_ + particle_radius_ * 0.25f,
                planetary_water_surface_radius_ - particle_radius_ * 0.2f);
            ++planetary_respawn_cursor_;
            const glm::vec3 respawn_simulation_normal = glm::normalize(simulation_from_surface * respawn_surface_normal);
            const glm::vec3 respawn_position = planetary_center_ + respawn_simulation_normal * respawn_radius;
            particle.position = glm::vec4(respawn_position, particle.position.w);
            particle.predicted_position = particle.position;
            particle.velocity = glm::vec4(0.f);
            particle.delta_position = glm::vec4(0.f);
            particle.solver_data = glm::vec4(0.f);
            changed = true;
        }

        hydrology_classification_timing_.add_sample((glfwGetTime() - classification_start) * 1000.0);
    }

    if (changed) {
        const double upload_start = glfwGetTime();
        compute_shader_->update_ssbo_indices(particle_binding_, respawn_indices, particles);
        respawn_ssbo_upload_timing_.add_sample((glfwGetTime() - upload_start) * 1000.0);
    }

    respawn_total_timing_.add_sample((glfwGetTime() - respawn_start) * 1000.0);
}

void gpu_fluid_system_component::collect_render_flood_mask_gpu_timing() const {
    if (!render_flood_mask_gpu_query_pending_ || render_flood_mask_gpu_query_ == 0)
        return;

    GLint available = 0;
    glGetQueryObjectiv(render_flood_mask_gpu_query_, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available == 0)
        return;

    GLuint64 elapsed_ns = 0;
    glGetQueryObjectui64v(render_flood_mask_gpu_query_, GL_QUERY_RESULT, &elapsed_ns);
    render_flood_mask_gpu_query_pending_ = false;
    render_flood_mask_gpu_timing_.add_sample(static_cast<double>(elapsed_ns) / 1000000.0);
}

void gpu_fluid_system_component::begin_render_flood_mask_gpu_timing() const {
    if (render_flood_mask_gpu_query_pending_)
        return;

    if (render_flood_mask_gpu_query_ == 0)
        glGenQueries(1, &render_flood_mask_gpu_query_);

    if (render_flood_mask_gpu_query_ == 0)
        return;

    glBeginQuery(GL_TIME_ELAPSED, render_flood_mask_gpu_query_);
    render_flood_mask_gpu_query_pending_ = true;
}

void gpu_fluid_system_component::end_render_flood_mask_gpu_timing() const {
    if (!render_flood_mask_gpu_query_pending_ || render_flood_mask_gpu_query_ == 0)
        return;

    glEndQuery(GL_TIME_ELAPSED);
}

void gpu_fluid_system_component::report_runtime_timing() {
    constexpr unsigned int report_interval_frames = 120u;
    ++runtime_timing_report_frames_;
    if (runtime_timing_report_frames_ < report_interval_frames)
        return;

    std::ostringstream stream;
    stream << "[gpu_fluid_profiler] avg over " << runtime_timing_report_frames_ << " frames"
        << " | fence_wait=" << gpu_completion_fence_wait_timing_.average_ms() << " ms (max " << gpu_completion_fence_wait_timing_.max_ms << ")"
        << " | respawn_total=" << respawn_total_timing_.average_ms() << " ms (max " << respawn_total_timing_.max_ms << ")"
        << " | ssbo_readback=" << respawn_ssbo_readback_timing_.average_ms() << " ms (max " << respawn_ssbo_readback_timing_.max_ms << ")"
        << " | hydrology_classification=" << hydrology_classification_timing_.average_ms() << " ms (max " << hydrology_classification_timing_.max_ms << ")"
        << " | ssbo_upload=" << respawn_ssbo_upload_timing_.average_ms() << " ms (max " << respawn_ssbo_upload_timing_.max_ms << ")";

    if (render_flood_mask_gpu_timing_.sample_count > 0u) {
        stream << " | render_flood_mask_gpu=" << render_flood_mask_gpu_timing_.average_ms()
            << " ms (max " << render_flood_mask_gpu_timing_.max_ms << ", samples " << render_flood_mask_gpu_timing_.sample_count << ")";
    }

    std::cout << stream.str() << std::endl;

    runtime_timing_report_frames_ = 0u;
    gpu_completion_fence_wait_timing_.reset();
    respawn_total_timing_.reset();
    respawn_ssbo_readback_timing_.reset();
    hydrology_classification_timing_.reset();
    respawn_ssbo_upload_timing_.reset();
    render_flood_mask_gpu_timing_.reset();
}

void gpu_fluid_system_component::set_debug_visualization_mode(fluid_debug_visualization_mode mode) {
    debug_visualization_mode_ = mode;
}

void gpu_fluid_system_component::set_debug_readback_enabled(bool enabled, unsigned int interval_frames) {
    debug_readback_enabled_ = enabled;
    debug_readback_interval_frames_ = std::max(1u, interval_frames);
    debug_readback_frame_counter_ = 0u;
}

void gpu_fluid_system_component::ensure_initialized() {
    if (initialized_ || !compute_shader_ || !compute_shader_->is_vaild())
        return;

    compute_shader_->use();
    compute_shader_->add_ssbo(particle_binding_, initial_particles_);
    rebuild_grid_buffers();
    initialized_ = true;
}

void gpu_fluid_system_component::update_planetary_angular_velocity(const glm::mat3& world_from_simulation, float dt) {
    planetary_angular_velocity_ = glm::vec3(0.f);
    if (dt <= 0.000001f)
        return;

    const glm::quat current_rotation = glm::normalize(glm::quat_cast(world_from_simulation));
    if (!previous_simulation_rotation_valid_) {
        previous_simulation_rotation_ = current_rotation;
        previous_simulation_rotation_valid_ = true;
        return;
    }

    glm::quat delta = current_rotation * glm::inverse(previous_simulation_rotation_);
    if (delta.w < 0.0f)
        delta = -delta;

    const float clamped_w = glm::clamp(delta.w, -1.0f, 1.0f);
    const float angle = 2.0f * std::acos(clamped_w);
    const float sin_half_angle = std::sqrt(glm::max(1.0f - clamped_w * clamped_w, 0.0f));
    if (angle > 0.000001f && sin_half_angle > 0.000001f) {
        const glm::vec3 axis_world(delta.x, delta.y, delta.z);
        const glm::vec3 normalized_axis_world = axis_world / sin_half_angle;
        const glm::vec3 angular_velocity_world = normalized_axis_world * (angle / dt) * planetary_rotation_rate_scale_;
        planetary_angular_velocity_ = glm::transpose(world_from_simulation) * angular_velocity_world;
    }

    previous_simulation_rotation_ = current_rotation;
}

rigid_body* gpu_fluid_system_component::find_planetary_host_body() const {
    auto* node = get_node();
    if (!node)
        return nullptr;

    auto bodies = node->find_component<rigid_body>(search_options::parent_self_first);
    return bodies.empty() ? nullptr : bodies.front();
}

int gpu_fluid_system_component::gather_planetary_external_gravity_sources(const glm::mat3& world_from_simulation) {
    planetary_external_gravity_sources_.fill(glm::vec4(0.f));
    if (!planetary_surface_enabled_)
        return 0;

    auto* host_body = find_planetary_host_body();
    auto* node = get_node();
    auto* scene_context = node ? dynamic_cast<scene*>(node->get_scene_manager()) : nullptr;
    if (!host_body || !scene_context || !scene_context->get_unit_system() || !scene_context->get_root_node())
        return 0;

    const float scaled_G = scene_context->get_unit_system()->scaled_G();
    const glm::vec3 host_world_position = host_body->get_position();
    std::vector<std::pair<float, glm::vec4>> ranked_sources;

    for (auto* body : scene_context->get_root_node()->find_component<rigid_body>(search_options::recursive_down)) {
        if (!body || body == host_body || body->get_mass() <= 0.0f)
            continue;

        const glm::vec3 delta_world = body->get_position() - host_world_position;
        const float distance_sq = glm::dot(delta_world, delta_world);
        if (distance_sq <= 0.000001f)
            continue;

        const float source_gm = scaled_G * body->get_mass();
        const float tidal_score = source_gm / (distance_sq * std::sqrt(distance_sq));
        const glm::vec3 source_position_simulation = planetary_center_ + glm::transpose(world_from_simulation) * delta_world;
        ranked_sources.emplace_back(tidal_score, glm::vec4(source_position_simulation, source_gm));
    }

    std::sort(ranked_sources.begin(), ranked_sources.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first > rhs.first;
    });

    const int source_count = static_cast<int>(std::min(ranked_sources.size(), planetary_external_gravity_sources_.size()));
    for (int i = 0; i < source_count; ++i)
        planetary_external_gravity_sources_[static_cast<size_t>(i)] = ranked_sources[static_cast<size_t>(i)].second;

    return source_count;
}

void gpu_fluid_system_component::fixed_update(float dt) {
    ensure_initialized();
    if (!initialized_ || !compute_shader_ || particle_count_ == 0 || cell_count_ == 0)
        return;

    update_adaptive_budget();

    glm::vec3 simulation_gravity = gravity_;
    glm::mat3 world_from_simulation(1.0f);
    if (const auto* node = get_node()) {
        const glm::mat4 model = node->get_global_matrix_model();
        glm::vec3 basis_x = glm::vec3(model[0]);
        glm::vec3 basis_y = glm::vec3(model[1]);
        glm::vec3 basis_z = glm::vec3(model[2]);

        if (glm::dot(basis_x, basis_x) > 0.000001f
            && glm::dot(basis_y, basis_y) > 0.000001f
            && glm::dot(basis_z, basis_z) > 0.000001f) {
            world_from_simulation = glm::mat3(
                glm::normalize(basis_x),
                glm::normalize(basis_y),
                glm::normalize(basis_z));
            simulation_gravity = glm::transpose(world_from_simulation) * gravity_;
        }
    }

    update_planetary_angular_velocity(world_from_simulation, dt);
    const int external_gravity_source_count = gather_planetary_external_gravity_sources(world_from_simulation);

    glm::mat3 planetary_surface_from_simulation(1.0f);
    if (planetary_surface_frame_node_) {
        const glm::mat4 surface_model = planetary_surface_frame_node_->get_global_matrix_model();
        glm::vec3 surface_basis_x = glm::vec3(surface_model[0]);
        glm::vec3 surface_basis_y = glm::vec3(surface_model[1]);
        glm::vec3 surface_basis_z = glm::vec3(surface_model[2]);
        if (glm::dot(surface_basis_x, surface_basis_x) > 0.000001f
            && glm::dot(surface_basis_y, surface_basis_y) > 0.000001f
            && glm::dot(surface_basis_z, surface_basis_z) > 0.000001f) {
            const glm::mat3 world_from_surface(
                glm::normalize(surface_basis_x),
                glm::normalize(surface_basis_y),
                glm::normalize(surface_basis_z));
            planetary_surface_from_simulation = glm::transpose(world_from_surface) * world_from_simulation;
        }
    }

    update_planetary_particle_respawn(planetary_surface_from_simulation);

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
    compute_shader_->set_uni_float("planetaryWaterSurfaceRadius", planetary_water_surface_radius_);
    compute_shader_->set_uni_float("planetaryGravityStrength", planetary_gravity_strength_);
    compute_shader_->set_uni_float("planetaryDownslopeStrength", planetary_downslope_strength_);
    compute_shader_->set_uni_float("planetaryBottomFriction", planetary_bottom_friction_);
    compute_shader_->set_uni_float("planetaryBottomNormalDamping", planetary_bottom_normal_damping_);
    compute_shader_->set_uni_float("planetaryFloorAttractionStrength", planetary_floor_attraction_strength_);
    compute_shader_->set_uni_float("planetaryFloodGuidanceStrength", planetary_flood_guidance_strength_);
    compute_shader_->set_uni_float("planetarySurfaceLayerThicknessScale", planetary_surface_layer_thickness_scale_);
    compute_shader_->set_uni_float("planetarySurfaceLayerAttractionStrength", planetary_surface_layer_attraction_strength_);
    compute_shader_->set_uni_float("planetarySurfaceLayerNormalVelocityDamping", planetary_surface_layer_normal_velocity_damping_);
    compute_shader_->set_uni_vec3("planetaryAngularVelocity", planetary_angular_velocity_);
    compute_shader_->set_uni_float("planetaryCoriolisStrength", planetary_coriolis_strength_);
    compute_shader_->set_uni_float("planetaryTidalStrength", planetary_tidal_strength_);
    compute_shader_->set_uni_int("planetaryExternalGravitySourceCount", external_gravity_source_count);
    compute_shader_->set_uni_vec4_array("planetaryExternalGravitySources", planetary_external_gravity_sources_.data(), max_planetary_external_gravity_sources_);
    compute_shader_->set_uni_int("planetaryPhysicsMaskTextureAvailable", planetary_physics_flood_mask_texture_ != 0 ? 1 : 0);
    compute_shader_->set_uni_int("planetaryPhysicsMaskTexture", 8);
    compute_shader_->set_uni_int("planetaryWaterLevelTextureAvailable", planetary_water_level_texture_ != 0 ? 1 : 0);
    compute_shader_->set_uni_int("planetaryWaterLevelTexture", 9);
    compute_shader_->set_uni_vec3("planetarySurfaceFrameX", planetary_surface_from_simulation[0]);
    compute_shader_->set_uni_vec3("planetarySurfaceFrameY", planetary_surface_from_simulation[1]);
    compute_shader_->set_uni_vec3("planetarySurfaceFrameZ", planetary_surface_from_simulation[2]);
    compute_shader_->set_uni_int("planetaryTerrainEnabled", planetary_terrain_enabled_ ? 1 : 0);
    compute_shader_->set_uni_float("terrainSeaLevel", planetary_terrain_profile_.sea_level);
    compute_shader_->set_uni_float("terrainContinentFrequency", planetary_terrain_profile_.continent_frequency);
    compute_shader_->set_uni_float("terrainContinentWarpStrength", planetary_terrain_profile_.continent_warp_strength);
    compute_shader_->set_uni_float("terrainLargeFrequency", planetary_terrain_profile_.large_frequency);
    compute_shader_->set_uni_float("terrainMediumFrequency", planetary_terrain_profile_.medium_frequency);
    compute_shader_->set_uni_float("terrainDetailFrequency", planetary_terrain_profile_.detail_frequency);
    compute_shader_->set_uni_float("terrainRidgeFrequency", planetary_terrain_profile_.ridge_frequency);
    compute_shader_->set_uni_float("terrainCraterStrength", planetary_terrain_profile_.crater_strength);
    compute_shader_->set_uni_float("terrainMountainSharpness", planetary_terrain_profile_.mountain_sharpness);
    compute_shader_->set_uni_float("terrainReliefStrength", planetary_terrain_profile_.relief_strength);
    compute_shader_->set_uni_float("terrainDisplacementStrength", planetary_terrain_profile_.displacement_strength);
    compute_shader_->set_uni_float("terrainContinentContrast", planetary_terrain_profile_.continent_contrast);
    compute_shader_->set_uni_float("terrainEarthMacroContinentStrength", planetary_terrain_profile_.earth_macro_continent_strength);
    compute_shader_->set_uni_float("terrainArchipelagoStrength", planetary_terrain_profile_.archipelago_strength);

    if (planetary_physics_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, planetary_physics_flood_mask_texture_);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, planetary_water_level_texture_);
        glActiveTexture(GL_TEXTURE0);
    }

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

    if (planetary_physics_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }

    queue_gpu_completion_fence();
    update_debug_readback();
    report_runtime_timing();
}

void gpu_fluid_system_component::update_debug_readback() {
    if (!debug_readback_enabled_ || !compute_shader_ || particle_count_ == 0)
        return;

    ++debug_readback_frame_counter_;
    if (debug_readback_frame_counter_ < debug_readback_interval_frames_)
        return;

    debug_readback_frame_counter_ = 0u;

    std::vector<fluid_particle> particles;
    compute_shader_->get_binding_data<fluid_particle>(particle_binding_, particles);

    if (particles.empty())
        return;

    constexpr size_t sample_count = 8u;
    const size_t stride = static_cast<size_t>(safe_sample_step(particles.size(), sample_count));
    float min_lambda = std::numeric_limits<float>::max();
    float max_lambda = std::numeric_limits<float>::lowest();
    float max_abs_constraint = 0.f;
    float avg_abs_constraint = 0.f;
    float min_distance = std::numeric_limits<float>::max();
    float max_distance = std::numeric_limits<float>::lowest();
    float avg_floor_clearance = 0.f;
    float avg_surface_depth = 0.f;
    glm::vec3 avg_velocity(0.f);
    float avg_coriolis_strength = 0.f;
    float avg_tidal_strength = 0.f;
    glm::vec3 avg_combined_flow(0.f);
    float max_speed = 0.f;
    size_t used_samples = 0u;

    glm::mat3 planetary_surface_from_simulation(1.0f);
    if (planetary_surface_frame_node_) {
        const glm::mat4 surface_model = planetary_surface_frame_node_->get_global_matrix_model();
        glm::vec3 surface_basis_x = glm::vec3(surface_model[0]);
        glm::vec3 surface_basis_y = glm::vec3(surface_model[1]);
        glm::vec3 surface_basis_z = glm::vec3(surface_model[2]);
        if (glm::dot(surface_basis_x, surface_basis_x) > 0.000001f
            && glm::dot(surface_basis_y, surface_basis_y) > 0.000001f
            && glm::dot(surface_basis_z, surface_basis_z) > 0.000001f) {
            const glm::mat3 world_from_surface(
                glm::normalize(surface_basis_x),
                glm::normalize(surface_basis_y),
                glm::normalize(surface_basis_z));
            planetary_surface_from_simulation = glm::transpose(world_from_surface);
        }
    }

    for (size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        const size_t particle_index = std::min(sample_index * stride, particles.size() - 1u);
        const fluid_particle& particle = particles[particle_index];
        const float lambda = particle.solver_data.x;
        const float abs_constraint = std::abs(particle.solver_data.z);
        const float distance = glm::length(glm::vec3(particle.position) - planetary_center_);
        const glm::vec3 velocity = glm::vec3(particle.velocity);
        const float speed = glm::length(velocity);
        const glm::vec3 surface_position = planetary_surface_from_simulation * glm::vec3(particle.position);
        const glm::vec3 radial = surface_position - planetary_center_;
        const float radial_distance = glm::length(radial);
        const glm::vec3 normal = radial_distance > 0.000001f ? radial / radial_distance : glm::vec3(0.f, 1.f, 0.f);
        const float floor_radius = planetary_radius_ + planet_terrain::terrain_surface_displacement_profiled(normal, planetary_terrain_profile_);
        const float floor_clearance = std::max(radial_distance - floor_radius, 0.f);
        const float surface_depth = std::max(planetary_water_surface_radius_ - radial_distance, 0.f);

        min_lambda = std::min(min_lambda, lambda);
        max_lambda = std::max(max_lambda, lambda);
        max_abs_constraint = std::max(max_abs_constraint, abs_constraint);
        avg_abs_constraint += abs_constraint;
        min_distance = std::min(min_distance, distance);
        max_distance = std::max(max_distance, distance);
        avg_floor_clearance += floor_clearance;
        avg_surface_depth += surface_depth;
        avg_velocity += velocity;
        avg_coriolis_strength += particle.solver_data.w;
        avg_tidal_strength += particle.debug_data.w;
        avg_combined_flow += glm::vec3(particle.debug_data);
        max_speed = std::max(max_speed, speed);
        ++used_samples;
    }

    if (used_samples == 0u)
        return;

    avg_abs_constraint /= static_cast<float>(used_samples);
    avg_floor_clearance /= static_cast<float>(used_samples);
    avg_surface_depth /= static_cast<float>(used_samples);
    avg_velocity /= static_cast<float>(used_samples);
    avg_coriolis_strength /= static_cast<float>(used_samples);
    avg_tidal_strength /= static_cast<float>(used_samples);
    avg_combined_flow /= static_cast<float>(used_samples);

    std::ostringstream stream;
    stream << "fluid debug samples=" << used_samples
        << " lambda[min=" << min_lambda << ", max=" << max_lambda << "]"
        << " |constraint|[avg=" << avg_abs_constraint << ", max=" << max_abs_constraint << "]"
        << " floorClearance[avg=" << avg_floor_clearance << "]"
        << " surfaceDepth[avg=" << avg_surface_depth << "]"
        << " velocity[avg=(" << avg_velocity.x << ", " << avg_velocity.y << ", " << avg_velocity.z << "), maxSpeed=" << max_speed << "]"
        << " coriolis[avg=" << avg_coriolis_strength << "]"
        << " tidal[avg=" << avg_tidal_strength << "]"
        << " combinedFlow[avg=(" << avg_combined_flow.x << ", " << avg_combined_flow.y << ", " << avg_combined_flow.z << ")]";

    if (planetary_surface_enabled_)
        stream << " radius[min=" << min_distance << ", max=" << max_distance << "]";

    std::cout << stream.str() << std::endl;
}

void gpu_fluid_system_component::draw(Camera* camera, GLuint scene_depth_texture) const {
    if (!camera || !render_shader_ || !render_mesh_ || !compute_shader_ || particle_count_ == 0)
        return;

    const GLuint ssbo = compute_shader_->get_ssbo_id(particle_binding_);
    if (ssbo == 0)
        return;

    const glm::mat4& system_model = get_node()->get_global_matrix_model();
    const bool enable_render_flood_mask = debug_visualization_mode_ == fluid_debug_visualization_mode::ocean_fill;
    const bool profile_render_flood_mask = enable_render_flood_mask && planetary_surface_enabled_ && planetary_flood_mask_texture_ != 0;

    collect_render_flood_mask_gpu_timing();

    render_shader_->use();
    render_shader_->set_uniform_mat4("systemModel", system_model);
    render_shader_->set_uniform_mat4("view", camera->GetViewMatrix());
    render_shader_->set_uniform_mat4("projection", camera->GetProjectionMatrix(build_aspect_ratio()));
    render_shader_->set_uni_float("particleSize", particle_size_);
    render_shader_->set_uni_float("particleRadius", particle_radius_);
    render_shader_->set_uni_float("viewportHeight", build_framebuffer_height());
    const bool render_as_point_sprites = render_mesh_->type == MeshType::POINTS;
    render_shader_->set_uni_int("renderPrimitiveMode", render_as_point_sprites ? 0 : 1);
    render_shader_->set_uni_int("surfaceInputPass", 0);
    render_shader_->set_uni_float("surfaceOpacity", render_as_point_sprites ? 1.35f : 0.92f);
    render_shader_->set_uni_vec3("particleColor", glm::vec3(0.18f, 0.58f, 1.0f));
    render_shader_->set_uni_int("useSceneDepth", scene_depth_texture != 0 ? 1 : 0);
    render_shader_->set_uni_int("sceneDepthTexture", 7);
    render_shader_->set_uni_int("planetaryPhysicsMaskTextureAvailable", planetary_physics_flood_mask_texture_ != 0 ? 1 : 0);
    render_shader_->set_uni_int("planetaryPhysicsMaskTexture", 8);
    render_shader_->set_uni_int("planetaryRenderMaskTextureAvailable", planetary_flood_mask_texture_ != 0 ? 1 : 0);
    render_shader_->set_uni_int("planetaryRenderMaskTexture", 9);
    render_shader_->set_uni_int("planetaryWaterLevelTextureAvailable", planetary_water_level_texture_ != 0 ? 1 : 0);
    render_shader_->set_uni_int("planetaryWaterLevelTexture", 10);
    render_shader_->set_uni_int("debugVisualizationMode", static_cast<int>(debug_visualization_mode_));
    render_shader_->set_uni_int("simulationMode", planetary_surface_enabled_ ? 1 : 0);
    render_shader_->set_uni_vec3("planetaryCenter", planetary_center_);
    render_shader_->set_uni_float("planetaryRadius", planetary_radius_);
    render_shader_->set_uni_float("planetaryShellThickness", planetary_shell_thickness_);
    render_shader_->set_uni_float("planetaryWaterSurfaceRadius", planetary_water_surface_radius_);
    render_shader_->set_uni_int("planetaryTerrainEnabled", planetary_terrain_enabled_ ? 1 : 0);
    render_shader_->set_uni_float("terrainSeaLevel", planetary_terrain_profile_.sea_level);
    render_shader_->set_uni_float("terrainContinentFrequency", planetary_terrain_profile_.continent_frequency);
    render_shader_->set_uni_float("terrainContinentWarpStrength", planetary_terrain_profile_.continent_warp_strength);
    render_shader_->set_uni_float("terrainLargeFrequency", planetary_terrain_profile_.large_frequency);
    render_shader_->set_uni_float("terrainMediumFrequency", planetary_terrain_profile_.medium_frequency);
    render_shader_->set_uni_float("terrainDetailFrequency", planetary_terrain_profile_.detail_frequency);
    render_shader_->set_uni_float("terrainRidgeFrequency", planetary_terrain_profile_.ridge_frequency);
    render_shader_->set_uni_float("terrainCraterStrength", planetary_terrain_profile_.crater_strength);
    render_shader_->set_uni_float("terrainMountainSharpness", planetary_terrain_profile_.mountain_sharpness);
    render_shader_->set_uni_float("terrainReliefStrength", planetary_terrain_profile_.relief_strength);
    render_shader_->set_uni_float("terrainDisplacementStrength", planetary_terrain_profile_.displacement_strength);
    render_shader_->set_uni_float("terrainContinentContrast", planetary_terrain_profile_.continent_contrast);
    render_shader_->set_uni_float("terrainEarthMacroContinentStrength", planetary_terrain_profile_.earth_macro_continent_strength);
    render_shader_->set_uni_float("terrainArchipelagoStrength", planetary_terrain_profile_.archipelago_strength);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    if (scene_depth_texture != 0) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, scene_depth_texture);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_physics_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, planetary_physics_flood_mask_texture_);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, planetary_flood_mask_texture_);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, planetary_water_level_texture_);
        glActiveTexture(GL_TEXTURE0);
    }

    if (render_as_point_sprites) {
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
    else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glDepthMask(GL_FALSE);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, ssbo);
    if (profile_render_flood_mask)
        begin_render_flood_mask_gpu_timing();

    render_mesh_->DrawInstanced(static_cast<GLsizei>(particle_count_));

    if (profile_render_flood_mask)
        end_render_flood_mask_gpu_timing();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, 0);
    if (scene_depth_texture != 0) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_physics_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);

}

void gpu_fluid_system_component::draw_particle_surface_input(Camera* camera, GLuint scene_depth_texture) const {
    if (!camera || !render_shader_ || !render_mesh_ || !compute_shader_ || particle_count_ == 0)
        return;

    const GLuint ssbo = compute_shader_->get_ssbo_id(particle_binding_);
    if (ssbo == 0)
        return;

    const glm::mat4& system_model = get_node()->get_global_matrix_model();
    render_shader_->use();
    render_shader_->set_uniform_mat4("systemModel", system_model);
    render_shader_->set_uniform_mat4("view", camera->GetViewMatrix());
    render_shader_->set_uniform_mat4("projection", camera->GetProjectionMatrix(build_aspect_ratio()));
    render_shader_->set_uni_float("particleSize", particle_size_);
    render_shader_->set_uni_float("particleRadius", particle_radius_);
    render_shader_->set_uni_float("viewportHeight", build_framebuffer_height());
    render_shader_->set_uni_int("renderPrimitiveMode", 0);
    render_shader_->set_uni_int("surfaceInputPass", 1);
    render_shader_->set_uni_float("surfaceOpacity", 1.0f);
    render_shader_->set_uni_vec3("particleColor", glm::vec3(1.0f));
    render_shader_->set_uni_int("useSceneDepth", scene_depth_texture != 0 ? 1 : 0);
    render_shader_->set_uni_int("sceneDepthTexture", 7);
    render_shader_->set_uni_int("planetaryPhysicsMaskTextureAvailable", planetary_physics_flood_mask_texture_ != 0 ? 1 : 0);
    render_shader_->set_uni_int("planetaryPhysicsMaskTexture", 8);
    render_shader_->set_uni_int("planetaryRenderMaskTextureAvailable", planetary_flood_mask_texture_ != 0 ? 1 : 0);
    render_shader_->set_uni_int("planetaryRenderMaskTexture", 9);
    render_shader_->set_uni_int("planetaryWaterLevelTextureAvailable", planetary_water_level_texture_ != 0 ? 1 : 0);
    render_shader_->set_uni_int("planetaryWaterLevelTexture", 10);
    render_shader_->set_uni_int("debugVisualizationMode", static_cast<int>(fluid_debug_visualization_mode::none));
    render_shader_->set_uni_int("simulationMode", planetary_surface_enabled_ ? 1 : 0);
    render_shader_->set_uni_vec3("planetaryCenter", planetary_center_);
    render_shader_->set_uni_float("planetaryRadius", planetary_radius_);
    render_shader_->set_uni_float("planetaryShellThickness", planetary_shell_thickness_);
    render_shader_->set_uni_float("planetaryWaterSurfaceRadius", planetary_water_surface_radius_);
    render_shader_->set_uni_int("planetaryTerrainEnabled", planetary_terrain_enabled_ ? 1 : 0);
    render_shader_->set_uni_float("terrainSeaLevel", planetary_terrain_profile_.sea_level);
    render_shader_->set_uni_float("terrainContinentFrequency", planetary_terrain_profile_.continent_frequency);
    render_shader_->set_uni_float("terrainContinentWarpStrength", planetary_terrain_profile_.continent_warp_strength);
    render_shader_->set_uni_float("terrainLargeFrequency", planetary_terrain_profile_.large_frequency);
    render_shader_->set_uni_float("terrainMediumFrequency", planetary_terrain_profile_.medium_frequency);
    render_shader_->set_uni_float("terrainDetailFrequency", planetary_terrain_profile_.detail_frequency);
    render_shader_->set_uni_float("terrainRidgeFrequency", planetary_terrain_profile_.ridge_frequency);
    render_shader_->set_uni_float("terrainCraterStrength", planetary_terrain_profile_.crater_strength);
    render_shader_->set_uni_float("terrainMountainSharpness", planetary_terrain_profile_.mountain_sharpness);
    render_shader_->set_uni_float("terrainReliefStrength", planetary_terrain_profile_.relief_strength);
    render_shader_->set_uni_float("terrainDisplacementStrength", planetary_terrain_profile_.displacement_strength);
    render_shader_->set_uni_float("terrainContinentContrast", planetary_terrain_profile_.continent_contrast);
    render_shader_->set_uni_float("terrainEarthMacroContinentStrength", planetary_terrain_profile_.earth_macro_continent_strength);
    render_shader_->set_uni_float("terrainArchipelagoStrength", planetary_terrain_profile_.archipelago_strength);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendEquationi(0, GL_FUNC_ADD);
    glBlendFunci(1, GL_ONE, GL_ONE);
    glBlendEquationi(1, GL_MIN);
    glDepthMask(GL_TRUE);

    if (scene_depth_texture != 0) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, scene_depth_texture);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_physics_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, planetary_physics_flood_mask_texture_);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, planetary_flood_mask_texture_);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, planetary_water_level_texture_);
        glActiveTexture(GL_TEXTURE0);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, ssbo);
    render_mesh_->DrawInstanced(static_cast<GLsizei>(particle_count_));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, 0);

    if (scene_depth_texture != 0) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_physics_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void gpu_fluid_system_component::draw_planetary_water_atlas_input(shader* atlas_shader, const glm::ivec2& atlas_resolution) const {
    if (!atlas_shader || !render_mesh_ || !compute_shader_ || particle_count_ == 0 || render_mesh_->type != MeshType::POINTS)
        return;

    const GLuint ssbo = compute_shader_->get_ssbo_id(particle_binding_);
    if (ssbo == 0)
        return;

    atlas_shader->use();
    atlas_shader->set_uni_vec2("atlasResolution", glm::vec2(atlas_resolution));
    atlas_shader->set_uni_float("particleRadius", particle_radius_);
    atlas_shader->set_uni_float("particleSize", particle_size_);
    atlas_shader->set_uni_vec3("planetaryCenter", planetary_center_);
    atlas_shader->set_uni_float("planetaryRadius", planetary_radius_);
    atlas_shader->set_uni_float("planetaryShellThickness", planetary_shell_thickness_);
    atlas_shader->set_uni_float("planetaryWaterSurfaceRadius", planetary_water_surface_radius_);
    atlas_shader->set_uni_int("planetaryTerrainEnabled", planetary_terrain_enabled_ ? 1 : 0);
    atlas_shader->set_uni_int("planetaryRenderMaskTextureAvailable", planetary_flood_mask_texture_ != 0 ? 1 : 0);
    atlas_shader->set_uni_int("planetaryRenderMaskTexture", 9);
    atlas_shader->set_uni_int("planetaryWaterLevelTextureAvailable", planetary_water_level_texture_ != 0 ? 1 : 0);
    atlas_shader->set_uni_int("planetaryWaterLevelTexture", 10);

    if (planetary_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, planetary_flood_mask_texture_);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, planetary_water_level_texture_);
        glActiveTexture(GL_TEXTURE0);
    }

    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, ssbo);
    render_mesh_->DrawInstanced(static_cast<GLsizei>(particle_count_));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, particle_binding_, 0);

    if (planetary_flood_mask_texture_ != 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
    if (planetary_water_level_texture_ != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }

    glDisable(GL_PROGRAM_POINT_SIZE);
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
