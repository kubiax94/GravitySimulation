#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <queue>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Mesh.h"
#include "Shader.h"
#include "fluid_particle.h"

namespace planet_terrain {
constexpr size_t ocean_fill_neighbor_count = 12u;

struct rocky_planet_profile
{
    float sea_level = 0.56f;
    float continent_frequency = 1.85f;
    float continent_warp_strength = 0.22f;
    float biome_frequency = 3.2f;
    float large_frequency = 4.2f;
    float medium_frequency = 9.5f;
    float detail_frequency = 18.0f;
    float ridge_frequency = 13.0f;
    float crater_strength = 0.18f;
    float mountain_sharpness = 0.82f;
    float relief_strength = 2.1f;
    float displacement_strength = 0.03f;
    float ocean_coverage = 0.0f;
    float ocean_motion_scale = 0.028f;
    size_t ocean_candidate_multiplier = 6u;
    glm::vec3 rock_dark_color = glm::vec3(0.12f, 0.08f, 0.05f);
    glm::vec3 rock_mid_color = glm::vec3(0.38f, 0.24f, 0.13f);
    glm::vec3 rock_bright_color = glm::vec3(0.73f, 0.55f, 0.33f);
    glm::vec3 dust_color = glm::vec3(0.61f, 0.33f, 0.15f);
    glm::vec3 ice_color = glm::vec3(0.76f, 0.84f, 0.92f);
    glm::vec3 shallow_ocean_color = glm::vec3(0.16f, 0.36f, 0.68f);
    glm::vec3 deep_ocean_color = glm::vec3(0.03f, 0.12f, 0.30f);
    glm::vec3 vegetation_color = glm::vec3(0.18f, 0.36f, 0.14f);
    glm::vec3 coast_color = glm::vec3(0.78f, 0.70f, 0.44f);
    glm::vec3 interior_color = glm::vec3(0.34f, 0.30f, 0.18f);
    glm::vec3 mountain_color = glm::vec3(0.62f, 0.58f, 0.50f);
    float ocean_visibility = 0.0f;
    bool static_ocean_tint_enabled = true;
    float vegetation_strength = 0.0f;
    float continent_contrast = 1.0f;
    float earth_macro_continent_strength = 0.0f;
    float archipelago_strength = 0.0f;
    int terrain_debug_mode = 0;
};

inline float terrain_height_profiled(const glm::vec3& n, const rocky_planet_profile& profile);
inline void apply_ocean_flood_debug(shader& rocky_shader, const rocky_planet_profile& profile);

inline rocky_planet_profile make_rocky_planet_profile(std::string_view planet_name) {
    rocky_planet_profile profile;
    if (planet_name == "Mercury") {
        profile.sea_level = 0.28f;
        profile.continent_frequency = 2.15f;
        profile.continent_warp_strength = 0.12f;
        profile.biome_frequency = 4.3f;
        profile.large_frequency = 5.8f;
        profile.medium_frequency = 13.0f;
        profile.detail_frequency = 23.5f;
        profile.ridge_frequency = 16.0f;
        profile.crater_strength = 0.34f;
        profile.mountain_sharpness = 0.72f;
        profile.relief_strength = 1.95f;
        profile.displacement_strength = 0.042f;
        profile.ocean_motion_scale = 0.01f;
        profile.ocean_candidate_multiplier = 4u;
        profile.rock_dark_color = glm::vec3(0.16f, 0.14f, 0.12f);
        profile.rock_mid_color = glm::vec3(0.34f, 0.30f, 0.25f);
        profile.rock_bright_color = glm::vec3(0.58f, 0.53f, 0.45f);
        profile.dust_color = glm::vec3(0.46f, 0.39f, 0.32f);
        profile.ice_color = glm::vec3(0.72f, 0.74f, 0.76f);
        profile.continent_contrast = 1.2f;
    }
    else if (planet_name == "Venus") {
        profile.sea_level = 0.54f;
        profile.continent_frequency = 0.98f;
        profile.continent_warp_strength = 0.46f;
        profile.biome_frequency = 1.25f;
        profile.large_frequency = 3.1f;
        profile.medium_frequency = 6.4f;
        profile.detail_frequency = 12.5f;
        profile.ridge_frequency = 8.4f;
        profile.crater_strength = 0.04f;
        profile.mountain_sharpness = 0.7f;
        profile.relief_strength = 0.95f;
        profile.displacement_strength = 0.016f;
        profile.ocean_motion_scale = 0.02f;
        profile.rock_dark_color = glm::vec3(0.30f, 0.21f, 0.12f);
        profile.rock_mid_color = glm::vec3(0.58f, 0.42f, 0.21f);
        profile.rock_bright_color = glm::vec3(0.86f, 0.72f, 0.42f);
        profile.dust_color = glm::vec3(0.92f, 0.82f, 0.58f);
        profile.ice_color = glm::vec3(0.95f, 0.90f, 0.70f);
        profile.continent_contrast = 0.78f;
    }
    else if (planet_name == "Earth") {
        profile.sea_level = 0.6f;
        profile.continent_frequency = 0.78f;
        profile.continent_warp_strength = 0.94f;
        profile.biome_frequency = 1.7f;
        profile.large_frequency = 2.9f;
        profile.medium_frequency = 4.1f;
        profile.detail_frequency = 7.2f;
        profile.ridge_frequency = 6.4f;
        profile.crater_strength = 0.035f;
        profile.mountain_sharpness = 0.73f;
        profile.relief_strength = 1.7f;
        profile.displacement_strength = 0.055f;
        profile.ocean_coverage = 0.8f;
        profile.ocean_motion_scale = 0.032f;
        profile.ocean_candidate_multiplier = 7u;
        profile.rock_dark_color = glm::vec3(0.10f, 0.18f, 0.09f);
        profile.rock_mid_color = glm::vec3(0.22f, 0.36f, 0.16f);
        profile.rock_bright_color = glm::vec3(0.54f, 0.48f, 0.30f);
        profile.dust_color = glm::vec3(0.70f, 0.62f, 0.38f);
        profile.ice_color = glm::vec3(0.86f, 0.92f, 0.96f);
        profile.shallow_ocean_color = glm::vec3(0.10f, 0.34f, 0.72f);
        profile.deep_ocean_color = glm::vec3(0.02f, 0.10f, 0.26f);
        profile.vegetation_color = glm::vec3(0.12f, 0.33f, 0.10f);
        profile.coast_color = glm::vec3(0.82f, 0.74f, 0.50f);
        profile.interior_color = glm::vec3(0.44f, 0.40f, 0.22f);
        profile.mountain_color = glm::vec3(0.58f, 0.56f, 0.52f);
        profile.ocean_visibility = 1.0f;
        profile.vegetation_strength = 0.9f;
        profile.continent_contrast = 1.1f;
        profile.earth_macro_continent_strength = 1.0f;
        profile.archipelago_strength = 0.55f;
    }
    else if (planet_name == "Mars") {
        profile.sea_level = 0.44f;
        profile.continent_frequency = 1.35f;
        profile.continent_warp_strength = 0.28f;
        profile.biome_frequency = 2.2f;
        profile.large_frequency = 5.0f;
        profile.medium_frequency = 11.2f;
        profile.detail_frequency = 20.0f;
        profile.ridge_frequency = 14.8f;
        profile.crater_strength = 0.2f;
        profile.mountain_sharpness = 0.88f;
        profile.relief_strength = 1.9f;
        profile.displacement_strength = 0.052f;
        profile.ocean_motion_scale = 0.016f;
        profile.rock_dark_color = glm::vec3(0.20f, 0.08f, 0.05f);
        profile.rock_mid_color = glm::vec3(0.48f, 0.18f, 0.10f);
        profile.rock_bright_color = glm::vec3(0.73f, 0.35f, 0.18f);
        profile.dust_color = glm::vec3(0.80f, 0.44f, 0.24f);
        profile.ice_color = glm::vec3(0.84f, 0.88f, 0.90f);
        profile.continent_contrast = 1.28f;
    }

    return profile;
}

inline void apply_rocky_planet_profile(shader& rocky_shader, const rocky_planet_profile& profile) {
    rocky_shader.use();
    rocky_shader.set_uni_float("terrainSeaLevel", profile.sea_level);
    rocky_shader.set_uni_float("terrainContinentFrequency", profile.continent_frequency);
    rocky_shader.set_uni_float("terrainContinentWarpStrength", profile.continent_warp_strength);
    rocky_shader.set_uni_float("terrainBiomeFrequency", profile.biome_frequency);
    rocky_shader.set_uni_float("terrainLargeFrequency", profile.large_frequency);
    rocky_shader.set_uni_float("terrainMediumFrequency", profile.medium_frequency);
    rocky_shader.set_uni_float("terrainDetailFrequency", profile.detail_frequency);
    rocky_shader.set_uni_float("terrainRidgeFrequency", profile.ridge_frequency);
    rocky_shader.set_uni_float("terrainCraterStrength", profile.crater_strength);
    rocky_shader.set_uni_float("terrainMountainSharpness", profile.mountain_sharpness);
    rocky_shader.set_uni_float("terrainReliefStrength", profile.relief_strength);
    rocky_shader.set_uni_float("terrainDisplacementStrength", profile.displacement_strength);
    rocky_shader.set_uni_vec3("terrainRockDarkColor", profile.rock_dark_color);
    rocky_shader.set_uni_vec3("terrainRockMidColor", profile.rock_mid_color);
    rocky_shader.set_uni_vec3("terrainRockBrightColor", profile.rock_bright_color);
    rocky_shader.set_uni_vec3("terrainDustColor", profile.dust_color);
    rocky_shader.set_uni_vec3("terrainIceColor", profile.ice_color);
    rocky_shader.set_uni_vec3("terrainShallowOceanColor", profile.shallow_ocean_color);
    rocky_shader.set_uni_vec3("terrainDeepOceanColor", profile.deep_ocean_color);
    rocky_shader.set_uni_vec3("terrainVegetationColor", profile.vegetation_color);
    rocky_shader.set_uni_vec3("terrainCoastColor", profile.coast_color);
    rocky_shader.set_uni_vec3("terrainInteriorColor", profile.interior_color);
    rocky_shader.set_uni_vec3("terrainMountainColor", profile.mountain_color);
    rocky_shader.set_uni_float("terrainOceanVisibility", profile.ocean_visibility);
    rocky_shader.set_uni_int("terrainStaticOceanTintEnabled", profile.static_ocean_tint_enabled ? 1 : 0);
    rocky_shader.set_uni_float("terrainVegetationStrength", profile.vegetation_strength);
    rocky_shader.set_uni_float("terrainContinentContrast", profile.continent_contrast);
    rocky_shader.set_uni_float("terrainEarthMacroContinentStrength", profile.earth_macro_continent_strength);
    rocky_shader.set_uni_float("terrainArchipelagoStrength", profile.archipelago_strength);
    rocky_shader.set_uni_int("terrainDebugMode", profile.terrain_debug_mode);
    apply_ocean_flood_debug(rocky_shader, profile);
}

inline float wave_noise(const glm::vec3& p) {
    float n = 0.0f;
    n += std::sin(p.x * 2.7f + p.y * 3.4f + p.z * 2.1f);
    n += 0.5f * std::sin(-p.x * 5.8f + p.y * 4.9f + p.z * 6.2f);
    n += 0.25f * std::sin(p.x * 10.7f - p.y * 9.1f + p.z * 7.5f);
    return n / 1.75f;
}

inline float fbm(glm::vec3 p) {
    float value = 0.0f;
    float amplitude = 0.55f;
    float frequency = 1.0f;

    for (int i = 0; i < 5; ++i) {
        value += amplitude * wave_noise(p * frequency);
        frequency *= 1.95f;
        amplitude *= 0.5f;
        p = glm::vec3(p.y, p.z, p.x) + glm::vec3(0.37f, -0.21f, 0.43f);
    }

    return value;
}

inline float crater_mask(const glm::vec3& p) {
    const float a = 0.5f + 0.5f * std::sin(p.x * 18.0f + p.y * 11.0f + p.z * 14.0f);
    const float b = 0.5f + 0.5f * std::sin(-p.x * 23.0f + p.y * 19.0f - p.z * 17.0f);
    const float c = 0.5f + 0.5f * std::sin(p.x * 29.0f - p.y * 27.0f + p.z * 21.0f);
    return glm::smoothstep(0.78f, 0.97f, a * b * c);
}

inline float continent_blob(const glm::vec3& n, const glm::vec3& center, float inner_dot, float outer_dot) {
    return glm::smoothstep(inner_dot, outer_dot, glm::dot(n, glm::normalize(center)));
}

inline float earth_macro_continent_mask(const glm::vec3& n, const rocky_planet_profile& profile) {
    const float afro_eurasia = continent_blob(n, glm::vec3(0.82f, 0.18f, -0.54f), 0.44f, 0.76f);
    const float americas = continent_blob(n, glm::vec3(-0.78f, 0.08f, 0.34f), 0.46f, 0.78f);
    const float australasia = continent_blob(n, glm::vec3(0.18f, -0.42f, 0.88f), 0.58f, 0.84f);
    const float polar_land = continent_blob(n, glm::vec3(0.18f, 0.86f, 0.12f), 0.72f, 0.9f) * 0.22f;

    float macro_land = glm::max(afro_eurasia, americas);
    macro_land = glm::max(macro_land, australasia * 0.82f);
    macro_land = glm::max(macro_land, polar_land);

    const float island_noise = 0.5f + 0.5f * fbm(glm::vec3(n.z, n.x, n.y) * 5.6f + glm::vec3(0.9f, -1.4f, 0.3f));
    const float coastal_band = glm::smoothstep(0.22f, 0.58f, macro_land) * (1.0f - glm::smoothstep(0.62f, 0.9f, macro_land));
    const float archipelagos = coastal_band * glm::smoothstep(0.56f, 0.82f, island_noise) * profile.archipelago_strength;

    return glm::clamp(glm::max(macro_land, archipelagos), 0.0f, 1.0f);
}

inline float continent_mask(const glm::vec3& n, const rocky_planet_profile& profile) {
    const glm::vec3 warped = glm::normalize(n + glm::vec3(
        fbm(n * (profile.continent_frequency * 1.2f) + glm::vec3(0.7f, -1.1f, 0.4f)),
        fbm(glm::vec3(n.z, n.x, n.y) * (profile.continent_frequency * 1.35f) + glm::vec3(-0.3f, 0.6f, -0.8f)),
        fbm(glm::vec3(n.y, n.z, n.x) * (profile.continent_frequency * 1.5f) + glm::vec3(1.0f, -0.2f, 0.9f)))
        * profile.continent_warp_strength);
    const float primary = 0.5f + 0.5f * fbm(warped * profile.continent_frequency + glm::vec3(1.3f, -0.9f, 0.6f));
    const float secondary = 0.5f + 0.5f * fbm(glm::vec3(warped.z, warped.x, warped.y) * (profile.continent_frequency * 2.05f) + glm::vec3(-1.2f, 0.4f, 1.1f));
    const float tertiary = 0.5f + 0.5f * fbm(glm::vec3(warped.y, warped.z, warped.x) * (profile.continent_frequency * 3.1f) + glm::vec3(0.5f, 1.0f, -0.7f));
    const float combined = primary * 0.68f + secondary * 0.24f + tertiary * 0.08f;
    const float generic_mask = glm::smoothstep(0.44f, 0.62f, combined);
    if (profile.earth_macro_continent_strength <= 0.001f)
        return generic_mask;

    const float macro_mask = earth_macro_continent_mask(n, profile);
    return glm::clamp(glm::mix(generic_mask, glm::max(generic_mask * 0.4f, macro_mask), profile.earth_macro_continent_strength), 0.0f, 1.0f);
}

inline float terrain_height(const glm::vec3& n) {
    return terrain_height_profiled(n, rocky_planet_profile{});
}

inline glm::vec3 fibonacci_direction(size_t index, size_t count);

struct ocean_fill_sample
{
    glm::vec3 normal = glm::vec3(0.f, 1.f, 0.f);
    float floor_radius = 0.f;
    float spill_radius = std::numeric_limits<float>::max();
    float ocean_fill = 0.f;
    int basin_index = -1;
    int downhill_neighbor_index = -1;
};

struct ocean_basin
{
    size_t minimum_sample_index = 0u;
    float minimum_floor_radius = 0.f;
    float spill_radius = std::numeric_limits<float>::max();
    std::vector<size_t> sample_indices;
};

struct ocean_basin_connection
{
    size_t basin_a = 0u;
    size_t basin_b = 0u;
    float spill_radius = std::numeric_limits<float>::max();
    size_t sample_index_a = 0u;
    size_t sample_index_b = 0u;
};

struct ocean_basin_graph
{
    std::vector<ocean_fill_sample> samples;
    std::vector<std::array<int, ocean_fill_neighbor_count>> neighbors;
    std::vector<size_t> minima_sample_indices;
    std::vector<ocean_basin> basins;
    std::vector<ocean_basin_connection> connections;
};

struct flooded_ocean_region
{
    std::vector<size_t> basin_indices;
    std::vector<size_t> sample_indices;
    float minimum_floor_radius = std::numeric_limits<float>::max();
    float water_surface_radius = 0.f;
};

struct ocean_flood_state
{
    float water_surface_radius = 0.f;
    std::vector<int> basin_region_indices;
    std::vector<int> sample_region_indices;
    std::vector<flooded_ocean_region> regions;
};

struct ocean_seed_generation_params
{
    size_t target_particle_count = 0u;
    float base_radius = 1.0f;
    float shell_thickness = 0.1f;
    float particle_radius = 0.026f;
    float coverage = -1.0f;
    size_t candidate_count = 0u;
    bool primary_regions_only = false;
};

struct ocean_seed_generation_result
{
    std::vector<fluid_particle> particles;
    ocean_basin_graph basin_graph;
    ocean_flood_state flood_state;
    float resolved_shell_thickness = 0.0f;
    float water_surface_radius = 0.0f;
    float flooded_volume_ratio = 0.0f;
    size_t effective_target_count = 0u;
};

using ocean_generation_progress_callback = std::function<void(float)>;

enum class cube_face : std::uint8_t
{
    positive_x,
    negative_x,
    positive_y,
    negative_y,
    positive_z,
    negative_z
};

struct terrain_patch_generation_params
{
    cube_face face = cube_face::positive_y;
    uint32_t resolution = 32u;
    glm::vec2 patch_min = glm::vec2(0.0f);
    glm::vec2 patch_max = glm::vec2(1.0f);
    float base_radius = 1.0f;
    bool apply_displacement = true;
};

using terrain_generation_progress_callback = std::function<void(float)>;

struct ocean_flood_debug_point
{
    glm::vec3 normal = glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 color = glm::vec3(0.05f);
    int region_index = -1;
};

constexpr size_t ocean_flood_debug_point_capacity = 256u;

inline float terrain_macro_height_profiled(const glm::vec3& n, const rocky_planet_profile& profile) {
    const float continents = continent_mask(n, profile);
    const float large_scale = 0.5f + 0.5f * fbm(n * profile.large_frequency);
    const float medium_scale = 0.5f + 0.5f * fbm(glm::vec3(n.z, n.x, n.y) * (profile.medium_frequency * 0.72f) + glm::vec3(1.7f, -2.1f, 0.9f));
    return large_scale * 0.72f
        + medium_scale * 0.18f
        + (continents - 0.46f) * 0.26f * profile.continent_contrast;
}

inline float earth_ocean_mask_profiled(const glm::vec3& n, float continents, float terrain, const rocky_planet_profile& profile) {
    const float basin_noise = 0.5f + 0.5f * fbm(glm::vec3(n.z, n.x, n.y) * 2.1f + glm::vec3(-0.7f, 0.5f, 1.2f));
    const float shelf_noise = 0.5f + 0.5f * fbm(glm::vec3(n.y, n.z, n.x) * 3.4f + glm::vec3(1.1f, -0.4f, 0.2f));
    const float basin_shape = glm::smoothstep(0.24f, 0.7f, 1.0f - continents) * (0.55f + 0.45f * basin_noise);
    const float sea_fill = glm::smoothstep(profile.sea_level + 0.02f, profile.sea_level - 0.1f, terrain);
    return glm::clamp(sea_fill * basin_shape * (0.65f + 0.35f * shelf_noise), 0.0f, 1.0f);
}

inline float ocean_fill_mask_profiled(const glm::vec3& n, const rocky_planet_profile& profile) {
    const float continents = continent_mask(n, profile);
    const float terrain = terrain_height_profiled(n, profile);
    float ocean_fill = glm::clamp((profile.sea_level - terrain) / 0.24f, 0.0f, 1.0f);
    if (profile.earth_macro_continent_strength > 0.001f)
        ocean_fill = glm::max(ocean_fill, earth_ocean_mask_profiled(n, continents, terrain, profile));

    return glm::smoothstep(0.02f, 0.18f, ocean_fill);
}

inline double compute_ocean_volume_proxy(const ocean_basin_graph& basin_graph, const ocean_flood_state& flood_state, float max_surface_radius) {
    double volume_proxy = 0.0;
    for (const flooded_ocean_region& region : flood_state.regions) {
        const float region_surface_radius = glm::clamp(region.water_surface_radius, 0.0f, max_surface_radius);
        for (const size_t sample_index : region.sample_indices) {
            const ocean_fill_sample& sample = basin_graph.samples[sample_index];
            const float filled_radius = glm::clamp(region_surface_radius, sample.floor_radius, max_surface_radius);
            if (filled_radius <= sample.floor_radius)
                continue;

            const double floor_radius_3 = static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius);
            const double filled_radius_3 = static_cast<double>(filled_radius) * static_cast<double>(filled_radius) * static_cast<double>(filled_radius);
            volume_proxy += filled_radius_3 - floor_radius_3;
        }
    }

    return volume_proxy;
}

inline double compute_shell_capacity_volume_proxy(const ocean_basin_graph& basin_graph, float max_surface_radius) {
    double volume_proxy = 0.0;
    for (const ocean_fill_sample& sample : basin_graph.samples) {
        const float filled_radius = glm::clamp(max_surface_radius, sample.floor_radius, max_surface_radius);
        if (filled_radius <= sample.floor_radius)
            continue;

        const double floor_radius_3 = static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius);
        const double filled_radius_3 = static_cast<double>(filled_radius) * static_cast<double>(filled_radius) * static_cast<double>(filled_radius);
        volume_proxy += filled_radius_3 - floor_radius_3;
    }

    return volume_proxy;
}

inline float terrain_height_profiled(const glm::vec3& n, const rocky_planet_profile& profile) {
    const float continents = continent_mask(n, profile);
    const float largeScale = 0.5f + 0.5f * fbm(n * profile.large_frequency);
    const float mediumScale = 0.5f + 0.5f * fbm(glm::vec3(n.z, n.x, n.y) * profile.medium_frequency + glm::vec3(1.7f, -2.1f, 0.9f));
    const float detailScale = 0.5f + 0.5f * fbm(glm::vec3(n.y, n.z, n.x) * profile.detail_frequency + glm::vec3(-3.2f, 1.4f, 2.6f));
    const float ridgeMask = std::pow(1.0f - std::abs(fbm(n * profile.ridge_frequency + glm::vec3(0.4f, -0.8f, 1.1f))), 2.3f);
    const float craters = crater_mask(n * 1.3f + glm::vec3(0.4f, -0.6f, 1.2f));

    const float height = largeScale * 0.55f
        + mediumScale * 0.27f
        + detailScale * 0.12f
        + ridgeMask * 0.06f
        + (continents - 0.46f) * 0.22f * profile.continent_contrast;

    return height - craters * profile.crater_strength;
}

inline float terrain_surface_displacement_profiled(const glm::vec3& n, const rocky_planet_profile& profile) {
    const float macro_height = terrain_macro_height_profiled(n, profile);
    const float full_height = terrain_height_profiled(n, profile);
    const float relief_strength = glm::max(profile.relief_strength, 0.01f);
    const float land_lift = glm::max(macro_height - profile.sea_level + 0.01f, 0.0f);
    const float land_relief = land_lift * (0.55f + 0.23f * relief_strength);
    const float ocean_shelf = -glm::max(profile.sea_level - macro_height, 0.0f) * 0.18f;
    const float mountain_relief = std::pow(glm::max(full_height - profile.mountain_sharpness, 0.0f), 1.15f) * 0.38f * relief_strength;
    const float displacement = (land_relief + ocean_shelf + mountain_relief) * profile.displacement_strength;
    const float max_upward_displacement = profile.displacement_strength * (0.55f + 0.40f * relief_strength);
    return glm::clamp(displacement, -profile.displacement_strength * 0.08f, max_upward_displacement);
}

inline glm::vec3 cube_face_point(cube_face face, const glm::vec2& patch_uv) {
    const glm::vec2 cube_uv = glm::clamp(patch_uv, glm::vec2(0.0f), glm::vec2(1.0f)) * 2.0f - glm::vec2(1.0f);

    switch (face) {
    case cube_face::positive_x:
        return glm::vec3(1.0f, -cube_uv.y, -cube_uv.x);
    case cube_face::negative_x:
        return glm::vec3(-1.0f, -cube_uv.y, cube_uv.x);
    case cube_face::positive_y:
        return glm::vec3(cube_uv.x, 1.0f, cube_uv.y);
    case cube_face::negative_y:
        return glm::vec3(cube_uv.x, -1.0f, -cube_uv.y);
    case cube_face::positive_z:
        return glm::vec3(cube_uv.x, -cube_uv.y, 1.0f);
    case cube_face::negative_z:
        return glm::vec3(-cube_uv.x, -cube_uv.y, -1.0f);
    }

    return glm::vec3(cube_uv.x, 1.0f, cube_uv.y);
}

inline glm::vec3 terrain_patch_surface_normal(cube_face face, const glm::vec2& patch_uv) {
    return glm::normalize(cube_face_point(face, patch_uv));
}

inline glm::vec3 terrain_patch_surface_position(cube_face face, const glm::vec2& patch_uv, float base_radius, const rocky_planet_profile& profile, bool apply_displacement) {
    const glm::vec3 normal = terrain_patch_surface_normal(face, patch_uv);
    const float displacement = apply_displacement ? terrain_surface_displacement_profiled(normal, profile) : 0.0f;
    return normal * (base_radius + displacement);
}

inline MeshData generate_terrain_patch_mesh(
    const terrain_patch_generation_params& params,
    const rocky_planet_profile& profile,
    const terrain_generation_progress_callback& progress_callback = {}) {
    MeshData mesh_data;
    const auto report_progress = [&](float value) {
        if (progress_callback)
            progress_callback(glm::clamp(value, 0.0f, 1.0f));
    };

    const uint32_t resolution = std::max<uint32_t>(1u, params.resolution);
    const uint32_t vertex_count_per_axis = resolution + 1u;
    mesh_data.vertecies.reserve(static_cast<size_t>(vertex_count_per_axis) * static_cast<size_t>(vertex_count_per_axis));
    mesh_data.indices.reserve(static_cast<size_t>(resolution) * static_cast<size_t>(resolution) * 6u);
    report_progress(0.0f);

    const glm::vec2 patch_span = params.patch_max - params.patch_min;
    for (uint32_t y = 0u; y <= resolution; ++y) {
        const float fy = static_cast<float>(y) / static_cast<float>(resolution);
        for (uint32_t x = 0u; x <= resolution; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(resolution);
            const glm::vec2 local_uv(fx, fy);
            const glm::vec2 patch_uv = params.patch_min + patch_span * local_uv;

            Vertex vertex{};
            vertex.Position = terrain_patch_surface_position(params.face, patch_uv, params.base_radius, profile, params.apply_displacement);
            vertex.Normal = terrain_patch_surface_normal(params.face, patch_uv);
            vertex.TextCoords = local_uv;
            mesh_data.vertecies.push_back(vertex);
        }

        report_progress(glm::mix(0.0f, 0.82f, static_cast<float>(y + 1u) / static_cast<float>(vertex_count_per_axis)));
    }

    for (uint32_t y = 0u; y < resolution; ++y) {
        for (uint32_t x = 0u; x < resolution; ++x) {
            const uint32_t i0 = y * vertex_count_per_axis + x;
            const uint32_t i1 = i0 + 1u;
            const uint32_t i2 = i0 + vertex_count_per_axis;
            const uint32_t i3 = i2 + 1u;

            mesh_data.indices.push_back(i0);
            mesh_data.indices.push_back(i2);
            mesh_data.indices.push_back(i1);

            mesh_data.indices.push_back(i1);
            mesh_data.indices.push_back(i2);
            mesh_data.indices.push_back(i3);
        }

        report_progress(glm::mix(0.82f, 1.0f, static_cast<float>(y + 1u) / static_cast<float>(resolution)));
    }

    report_progress(1.0f);
    return mesh_data;
}

inline std::vector<std::array<int, ocean_fill_neighbor_count>> build_fill_sample_neighbors(const std::vector<ocean_fill_sample>& samples) {
    const size_t sample_count = samples.size();
    std::vector<std::array<int, ocean_fill_neighbor_count>> neighbors(sample_count);
    std::vector<std::array<float, ocean_fill_neighbor_count>> neighbor_scores(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        neighbors[i].fill(-1);
        neighbor_scores[i].fill(-std::numeric_limits<float>::max());
    }

    const size_t grid_resolution = std::max<size_t>(4u, static_cast<size_t>(std::sqrt(static_cast<float>(sample_count) / 24.0f)));
    const size_t bucket_count = grid_resolution * grid_resolution * grid_resolution;
    std::vector<int> bucket_heads(bucket_count, -1);
    std::vector<int> bucket_next(sample_count, -1);

    const auto clamp_grid_coord = [grid_resolution](float v) {
        const float normalized = glm::clamp(v * 0.5f + 0.5f, 0.0f, 0.99999994f);
        const int coord = static_cast<int>(normalized * static_cast<float>(grid_resolution));
        return glm::clamp(coord, 0, static_cast<int>(grid_resolution) - 1);
    };

    const auto bucket_index = [grid_resolution](int x, int y, int z) {
        return static_cast<size_t>(x)
            + static_cast<size_t>(y) * grid_resolution
            + static_cast<size_t>(z) * grid_resolution * grid_resolution;
    };

    for (size_t i = 0; i < sample_count; ++i) {
        const glm::vec3& normal = samples[i].normal;
        const int cell_x = clamp_grid_coord(normal.x);
        const int cell_y = clamp_grid_coord(normal.y);
        const int cell_z = clamp_grid_coord(normal.z);
        const size_t cell_index = bucket_index(cell_x, cell_y, cell_z);
        bucket_next[i] = bucket_heads[cell_index];
        bucket_heads[cell_index] = static_cast<int>(i);
    }

    const auto insert_neighbor = [&](size_t owner, int candidate, float score) {
        auto& owner_neighbors = neighbors[owner];
        auto& owner_scores = neighbor_scores[owner];
        int replace_slot = -1;
        float lowest_score = std::numeric_limits<float>::max();
        for (size_t slot = 0; slot < ocean_fill_neighbor_count; ++slot) {
            if (owner_neighbors[slot] == candidate)
                return;

            if (owner_neighbors[slot] < 0) {
                replace_slot = static_cast<int>(slot);
                break;
            }

            if (owner_scores[slot] < lowest_score) {
                lowest_score = owner_scores[slot];
                replace_slot = static_cast<int>(slot);
            }
        }

        if (replace_slot < 0)
            return;

        if (owner_neighbors[replace_slot] >= 0 && owner_scores[replace_slot] >= score)
            return;

        owner_neighbors[replace_slot] = candidate;
        owner_scores[replace_slot] = score;
    };

    for (size_t i = 0; i < sample_count; ++i) {
        const glm::vec3& normal = samples[i].normal;
        const int cell_x = clamp_grid_coord(normal.x);
        const int cell_y = clamp_grid_coord(normal.y);
        const int cell_z = clamp_grid_coord(normal.z);

        for (int search_ring = 0; search_ring <= 3; ++search_ring) {
            bool has_open_slot = false;
            for (size_t slot = 0; slot < ocean_fill_neighbor_count; ++slot) {
                if (neighbors[i][slot] < 0) {
                    has_open_slot = true;
                    break;
                }
            }

            if (!has_open_slot && search_ring > 0)
                break;

            for (int dz = -search_ring; dz <= search_ring; ++dz) {
                const int z = cell_z + dz;
                if (z < 0 || z >= static_cast<int>(grid_resolution))
                    continue;

                for (int dy = -search_ring; dy <= search_ring; ++dy) {
                    const int y = cell_y + dy;
                    if (y < 0 || y >= static_cast<int>(grid_resolution))
                        continue;

                    for (int dx = -search_ring; dx <= search_ring; ++dx) {
                        if (search_ring > 0 && std::max({ std::abs(dx), std::abs(dy), std::abs(dz) }) != search_ring)
                            continue;

                        const int x = cell_x + dx;
                        if (x < 0 || x >= static_cast<int>(grid_resolution))
                            continue;

                        for (int candidate = bucket_heads[bucket_index(x, y, z)]; candidate >= 0; candidate = bucket_next[static_cast<size_t>(candidate)]) {
                            if (candidate == static_cast<int>(i))
                                continue;

                            const float score = glm::dot(normal, samples[static_cast<size_t>(candidate)].normal);
                            insert_neighbor(i, candidate, score);
                            insert_neighbor(static_cast<size_t>(candidate), static_cast<int>(i), score);
                        }
                    }
                }
            }
        }
    }

    return neighbors;
}

inline std::vector<size_t> collect_local_minima_indices(const std::vector<ocean_fill_sample>& samples, const std::vector<std::array<int, ocean_fill_neighbor_count>>& neighbors) {
    std::vector<size_t> minima_indices;
    minima_indices.reserve(samples.size() / 8u + 1u);
    constexpr float local_minimum_eps = 0.00001f;
    for (size_t i = 0; i < samples.size(); ++i) {
        bool is_local_minimum = true;
        for (size_t slot = 0; slot < ocean_fill_neighbor_count; ++slot) {
            const int neighbor_index = neighbors[i][slot];
            if (neighbor_index < 0)
                continue;

            if (samples[neighbor_index].floor_radius < samples[i].floor_radius - local_minimum_eps) {
                is_local_minimum = false;
                break;
            }
        }

        if (is_local_minimum)
            minima_indices.push_back(i);
    }

    if (minima_indices.empty() && !samples.empty()) {
        auto min_it = std::min_element(samples.begin(), samples.end(), [](const ocean_fill_sample& lhs, const ocean_fill_sample& rhs) {
            return lhs.floor_radius < rhs.floor_radius;
        });
        minima_indices.push_back(static_cast<size_t>(std::distance(samples.begin(), min_it)));
    }

    return minima_indices;
}

inline float estimate_fill_sample_barrier_radius(
    const ocean_fill_sample& sample_a,
    const ocean_fill_sample& sample_b,
    float base_radius,
    const rocky_planet_profile& profile) {
    float barrier_radius = std::max(sample_a.floor_radius, sample_b.floor_radius);
    constexpr float edge_probe_t_values[] = { 0.25f, 0.5f, 0.75f };
    for (const float t : edge_probe_t_values) {
        const glm::vec3 probe_normal = glm::normalize(glm::mix(sample_a.normal, sample_b.normal, t));
        barrier_radius = std::max(barrier_radius, base_radius + terrain_surface_displacement_profiled(probe_normal, profile));
    }

    return barrier_radius;
}

inline void compute_fill_sample_spill_radii(
    std::vector<ocean_fill_sample>& samples,
    const std::vector<std::array<int, ocean_fill_neighbor_count>>& neighbors,
    float base_radius,
    const rocky_planet_profile& profile) {
    constexpr float local_minimum_eps = 0.00001f;
    const std::vector<size_t> minima_indices = collect_local_minima_indices(samples, neighbors);

    using flood_state = std::pair<float, size_t>;
    std::priority_queue<flood_state, std::vector<flood_state>, std::greater<flood_state>> frontier;
    for (const size_t minimum_index : minima_indices) {
        samples[minimum_index].spill_radius = samples[minimum_index].floor_radius;
        frontier.push({ samples[minimum_index].spill_radius, minimum_index });
    }

    while (!frontier.empty()) {
        const auto [spill_radius, sample_index] = frontier.top();
        frontier.pop();
        if (spill_radius > samples[sample_index].spill_radius + local_minimum_eps)
            continue;

        for (size_t slot = 0; slot < ocean_fill_neighbor_count; ++slot) {
            const int neighbor_index = neighbors[sample_index][slot];
            if (neighbor_index < 0)
                continue;

            const float edge_barrier_radius = estimate_fill_sample_barrier_radius(
                samples[sample_index],
                samples[static_cast<size_t>(neighbor_index)],
                base_radius,
                profile);
            const float propagated_spill_radius = std::max(spill_radius, edge_barrier_radius);
            if (propagated_spill_radius + local_minimum_eps < samples[neighbor_index].spill_radius) {
                samples[neighbor_index].spill_radius = propagated_spill_radius;
                frontier.push({ propagated_spill_radius, static_cast<size_t>(neighbor_index) });
            }
        }
    }
}

inline std::vector<ocean_fill_sample> build_ocean_fill_samples(float base_radius, const rocky_planet_profile& profile, size_t sample_count = 4096u) {
    sample_count = std::max<size_t>(sample_count, 1u);
    std::vector<ocean_fill_sample> samples(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        const glm::vec3 normal = fibonacci_direction(i, sample_count);
        samples[i].normal = normal;
        samples[i].floor_radius = base_radius + terrain_surface_displacement_profiled(normal, profile);
        samples[i].ocean_fill = ocean_fill_mask_profiled(normal, profile);
    }

    const auto neighbors = build_fill_sample_neighbors(samples);
    compute_fill_sample_spill_radii(samples, neighbors, base_radius, profile);

    return samples;
}

inline ocean_basin_graph build_ocean_basin_graph(float base_radius, const rocky_planet_profile& profile, size_t sample_count = 4096u) {
    ocean_basin_graph graph;
    sample_count = std::max<size_t>(sample_count, 1u);
    graph.samples.resize(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        const glm::vec3 normal = fibonacci_direction(i, sample_count);
        graph.samples[i].normal = normal;
        graph.samples[i].floor_radius = base_radius + terrain_surface_displacement_profiled(normal, profile);
        graph.samples[i].ocean_fill = ocean_fill_mask_profiled(normal, profile);
    }

    graph.neighbors = build_fill_sample_neighbors(graph.samples);
    compute_fill_sample_spill_radii(graph.samples, graph.neighbors, base_radius, profile);
    graph.minima_sample_indices = collect_local_minima_indices(graph.samples, graph.neighbors);

    std::vector<int> sink_cache(sample_count, -2);
    const auto resolve_sink = [&](auto&& self, int sample_index) -> int {
        int& cached_sink = sink_cache[static_cast<size_t>(sample_index)];
        if (cached_sink >= -1)
            return cached_sink;

        float best_floor = graph.samples[static_cast<size_t>(sample_index)].floor_radius;
        int best_neighbor = -1;
        for (size_t slot = 0; slot < ocean_fill_neighbor_count; ++slot) {
            const int neighbor_index = graph.neighbors[static_cast<size_t>(sample_index)][slot];
            if (neighbor_index < 0)
                continue;

            if (graph.samples[static_cast<size_t>(neighbor_index)].floor_radius < best_floor) {
                best_floor = graph.samples[static_cast<size_t>(neighbor_index)].floor_radius;
                best_neighbor = neighbor_index;
            }
        }

        graph.samples[static_cast<size_t>(sample_index)].downhill_neighbor_index = best_neighbor;
        cached_sink = best_neighbor >= 0 ? self(self, best_neighbor) : sample_index;
        return cached_sink;
    };

    std::unordered_map<int, size_t> sink_to_basin_index;
    for (size_t i = 0; i < sample_count; ++i) {
        const int sink_index = resolve_sink(resolve_sink, static_cast<int>(i));
        const auto [it, inserted] = sink_to_basin_index.emplace(sink_index, graph.basins.size());
        if (inserted) {
            ocean_basin basin;
            basin.minimum_sample_index = static_cast<size_t>(sink_index);
            basin.minimum_floor_radius = graph.samples[static_cast<size_t>(sink_index)].floor_radius;
            graph.basins.push_back(std::move(basin));
        }

        const size_t basin_index = it->second;
        graph.samples[i].basin_index = static_cast<int>(basin_index);
        graph.basins[basin_index].sample_indices.push_back(i);
    }

    std::unordered_map<unsigned long long, size_t> connection_lookup;
    for (size_t i = 0; i < sample_count; ++i) {
        const int basin_index_a = graph.samples[i].basin_index;
        if (basin_index_a < 0)
            continue;

        for (size_t slot = 0; slot < ocean_fill_neighbor_count; ++slot) {
            const int neighbor_index = graph.neighbors[i][slot];
            if (neighbor_index < 0)
                continue;

            const int basin_index_b = graph.samples[static_cast<size_t>(neighbor_index)].basin_index;
            if (basin_index_b < 0 || basin_index_a == basin_index_b)
                continue;

            const size_t basin_a = static_cast<size_t>(std::min(basin_index_a, basin_index_b));
            const size_t basin_b = static_cast<size_t>(std::max(basin_index_a, basin_index_b));
            const unsigned long long key = (static_cast<unsigned long long>(basin_a) << 32ull) | static_cast<unsigned long long>(basin_b);
            const float spill_radius = estimate_fill_sample_barrier_radius(
                graph.samples[i],
                graph.samples[static_cast<size_t>(neighbor_index)],
                base_radius,
                profile);

            auto connection_it = connection_lookup.find(key);
            if (connection_it == connection_lookup.end()) {
                ocean_basin_connection connection;
                connection.basin_a = basin_a;
                connection.basin_b = basin_b;
                connection.spill_radius = spill_radius;
                connection.sample_index_a = i;
                connection.sample_index_b = static_cast<size_t>(neighbor_index);
                connection_lookup.emplace(key, graph.connections.size());
                graph.connections.push_back(std::move(connection));
            }
            else {
                ocean_basin_connection& connection = graph.connections[connection_it->second];
                if (spill_radius < connection.spill_radius) {
                    connection.spill_radius = spill_radius;
                    connection.sample_index_a = i;
                    connection.sample_index_b = static_cast<size_t>(neighbor_index);
                }
            }
        }
    }

    for (const ocean_basin_connection& connection : graph.connections) {
        graph.basins[connection.basin_a].spill_radius = std::min(graph.basins[connection.basin_a].spill_radius, connection.spill_radius);
        graph.basins[connection.basin_b].spill_radius = std::min(graph.basins[connection.basin_b].spill_radius, connection.spill_radius);
    }

    return graph;
}

inline float estimate_water_surface_radius_from_fill_samples(const std::vector<ocean_fill_sample>& samples, float base_radius, float shell_thickness, float particle_radius, float coverage) {
    if (coverage <= 0.0f || samples.empty())
        return base_radius + particle_radius * 1.5f;

    const float clamped_coverage = glm::clamp(coverage, 0.0f, 1.0f);
    const float max_surface_radius = base_radius + shell_thickness;
    const float min_surface_radius = [&]() {
        float min_radius = max_surface_radius;
        for (const ocean_fill_sample& sample : samples)
            min_radius = std::min(min_radius, sample.floor_radius);
        return min_radius;
    }();

    const auto volume_proxy_at_surface_radius = [&](float surface_radius) {
        double total_volume_proxy = 0.0;
        for (const ocean_fill_sample& sample : samples) {
            if (sample.spill_radius > surface_radius)
                continue;

            const double fill_weight = static_cast<double>(glm::smoothstep(0.14f, 0.82f, sample.ocean_fill));
            if (fill_weight <= 0.0)
                continue;

            const float filled_radius = glm::clamp(surface_radius, sample.floor_radius, max_surface_radius);
            if (filled_radius <= sample.floor_radius)
                continue;

            const double floor_radius_3 = static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius);
            const double filled_radius_3 = static_cast<double>(filled_radius) * static_cast<double>(filled_radius) * static_cast<double>(filled_radius);
            total_volume_proxy += (filled_radius_3 - floor_radius_3) * fill_weight;
        }

        return total_volume_proxy;
    };

    const double max_volume_proxy = volume_proxy_at_surface_radius(max_surface_radius);
    if (max_volume_proxy <= 0.0)
        return glm::clamp(max_surface_radius, base_radius + particle_radius * 0.5f, max_surface_radius);

    const double target_volume_proxy = static_cast<double>(clamped_coverage) * max_volume_proxy;
    float low = min_surface_radius;
    float high = max_surface_radius;
    for (int iteration = 0; iteration < 28; ++iteration) {
        const float mid = (low + high) * 0.5f;
        if (volume_proxy_at_surface_radius(mid) < target_volume_proxy)
            low = mid;
        else
            high = mid;
    }

    return glm::clamp(high, base_radius + particle_radius * 0.5f, max_surface_radius);
}

inline float estimate_water_surface_radius(float base_radius, float shell_thickness, float particle_radius, float coverage, const rocky_planet_profile& profile, size_t sample_count = 4096u) {
    const auto fill_samples = build_ocean_fill_samples(base_radius, profile, sample_count);
    return estimate_water_surface_radius_from_fill_samples(fill_samples, base_radius, shell_thickness, particle_radius, coverage);
}

inline ocean_flood_state build_ocean_flood_state(const ocean_basin_graph& basin_graph, float base_radius, float shell_thickness, float particle_radius, float coverage) {
    ocean_flood_state state;
    state.water_surface_radius = base_radius + particle_radius * 0.5f;
    state.basin_region_indices.assign(basin_graph.basins.size(), -1);
    state.sample_region_indices.assign(basin_graph.samples.size(), -1);

    if (basin_graph.basins.empty() || basin_graph.samples.empty())
        return state;

    const float clamped_coverage = glm::clamp(coverage, 0.0f, 1.0f);
    if (clamped_coverage <= 0.0f)
        return state;

    const float max_surface_radius = base_radius + shell_thickness;
    const auto flooded_volume_proxy_at_surface = [&](float surface_radius) {
        double volume_proxy = 0.0;
        for (const ocean_fill_sample& sample : basin_graph.samples) {
            if (sample.spill_radius > surface_radius)
                continue;

            const double fill_weight = static_cast<double>(glm::smoothstep(0.14f, 0.82f, sample.ocean_fill));
            if (fill_weight <= 0.0)
                continue;

            const float filled_radius = glm::clamp(surface_radius, sample.floor_radius, max_surface_radius);
            if (filled_radius <= sample.floor_radius)
                continue;

            const double floor_radius_3 = static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius) * static_cast<double>(sample.floor_radius);
            const double filled_radius_3 = static_cast<double>(filled_radius) * static_cast<double>(filled_radius) * static_cast<double>(filled_radius);
            volume_proxy += (filled_radius_3 - floor_radius_3) * fill_weight;
        }

        return volume_proxy;
    };

    const double max_volume_proxy = flooded_volume_proxy_at_surface(max_surface_radius);
    if (max_volume_proxy <= 0.0)
        return state;

    float min_surface_radius = max_surface_radius;
    for (const ocean_fill_sample& sample : basin_graph.samples)
        min_surface_radius = std::min(min_surface_radius, sample.floor_radius);

    const double target_volume_proxy = static_cast<double>(clamped_coverage) * max_volume_proxy;
    float low = min_surface_radius;
    float high = max_surface_radius;
    for (int iteration = 0; iteration < 28; ++iteration) {
        const float mid = (low + high) * 0.5f;
        if (flooded_volume_proxy_at_surface(mid) < target_volume_proxy)
            low = mid;
        else
            high = mid;
    }

    state.water_surface_radius = glm::clamp(high, base_radius + particle_radius * 0.5f, max_surface_radius);

    std::vector<unsigned char> flooded_samples(basin_graph.samples.size(), 0u);
    for (size_t sample_index = 0; sample_index < basin_graph.samples.size(); ++sample_index) {
        const ocean_fill_sample& sample = basin_graph.samples[sample_index];
        if (sample.spill_radius > state.water_surface_radius)
            continue;

        if (sample.floor_radius >= state.water_surface_radius)
            continue;

        if (sample.ocean_fill < 0.12f)
            continue;

        flooded_samples[sample_index] = 1u;
    }

    std::vector<unsigned char> basin_added(basin_graph.basins.size(), 0u);
    std::queue<size_t> frontier;
    for (size_t seed_sample_index = 0; seed_sample_index < basin_graph.samples.size(); ++seed_sample_index) {
        if (flooded_samples[seed_sample_index] == 0u || state.sample_region_indices[seed_sample_index] >= 0)
            continue;

        flooded_ocean_region region;
        region.water_surface_radius = state.water_surface_radius;

        while (!frontier.empty())
            frontier.pop();
        frontier.push(seed_sample_index);
        const int region_index = static_cast<int>(state.regions.size());

        while (!frontier.empty()) {
            const size_t sample_index = frontier.front();
            frontier.pop();
            if (state.sample_region_indices[sample_index] >= 0)
                continue;

            state.sample_region_indices[sample_index] = region_index;
            region.sample_indices.push_back(sample_index);

            const ocean_fill_sample& sample = basin_graph.samples[sample_index];
            region.minimum_floor_radius = std::min(region.minimum_floor_radius, sample.floor_radius);

            if (sample.basin_index >= 0) {
                const size_t basin_index = static_cast<size_t>(sample.basin_index);
                if (!basin_added[basin_index]) {
                    basin_added[basin_index] = 1u;
                    region.basin_indices.push_back(basin_index);
                }
                state.basin_region_indices[basin_index] = region_index;
            }

            for (size_t slot = 0; slot < ocean_fill_neighbor_count; ++slot) {
                const int neighbor_index = basin_graph.neighbors[sample_index][slot];
                if (neighbor_index < 0)
                    continue;

                const size_t neighbor_sample_index = static_cast<size_t>(neighbor_index);
                if (flooded_samples[neighbor_sample_index] == 0u || state.sample_region_indices[neighbor_sample_index] >= 0)
                    continue;

                frontier.push(neighbor_sample_index);
            }
        }

        for (const size_t basin_index : region.basin_indices)
            basin_added[basin_index] = 0u;

        if (!region.sample_indices.empty())
            state.regions.push_back(std::move(region));
    }

    return state;
}

inline ocean_flood_state build_ocean_flood_state(float base_radius, float shell_thickness, float particle_radius, float coverage, const rocky_planet_profile& profile, size_t sample_count = 4096u) {
    return build_ocean_flood_state(build_ocean_basin_graph(base_radius, profile, sample_count), base_radius, shell_thickness, particle_radius, coverage);
}

inline ocean_flood_state filter_to_primary_ocean_regions(
    const ocean_basin_graph& basin_graph,
    const ocean_flood_state& flood_state,
    float shell_thickness) {
    if (flood_state.regions.size() <= 1u)
        return flood_state;

    std::vector<float> region_scores(flood_state.regions.size(), 0.0f);
    float best_score = 0.0f;
    size_t best_region_index = 0u;
    for (size_t region_index = 0; region_index < flood_state.regions.size(); ++region_index) {
        const auto& region = flood_state.regions[region_index];
        if (region.sample_indices.empty())
            continue;

        float depth_sum = 0.0f;
        for (const size_t sample_index : region.sample_indices) {
            const auto& sample = basin_graph.samples[sample_index];
            depth_sum += glm::clamp(
                region.water_surface_radius - sample.floor_radius,
                0.0f,
                glm::max(shell_thickness, 0.0001f));
        }

        const float average_depth = depth_sum / static_cast<float>(region.sample_indices.size());
        const float score = static_cast<float>(region.sample_indices.size()) * (0.35f + average_depth / glm::max(shell_thickness, 0.0001f));
        region_scores[region_index] = score;
        if (score > best_score) {
            best_score = score;
            best_region_index = region_index;
        }
    }

    ocean_flood_state filtered_state;
    filtered_state.water_surface_radius = flood_state.water_surface_radius;
    filtered_state.basin_region_indices.assign(flood_state.basin_region_indices.size(), -1);
    filtered_state.sample_region_indices.assign(flood_state.sample_region_indices.size(), -1);

    const float score_threshold = best_score * 0.18f;
    const size_t sample_threshold = std::max<size_t>(32u, flood_state.regions[best_region_index].sample_indices.size() / 10u);
    for (size_t region_index = 0; region_index < flood_state.regions.size(); ++region_index) {
        const auto& region = flood_state.regions[region_index];
        const bool keep_region = region_index == best_region_index
            || (region.sample_indices.size() >= sample_threshold && region_scores[region_index] >= score_threshold);
        if (!keep_region)
            continue;

        const int filtered_region_index = static_cast<int>(filtered_state.regions.size());
        filtered_state.regions.push_back(region);
        for (const size_t sample_index : region.sample_indices)
            filtered_state.sample_region_indices[sample_index] = filtered_region_index;
        for (const size_t basin_index : region.basin_indices)
            filtered_state.basin_region_indices[basin_index] = filtered_region_index;
    }

    if (filtered_state.regions.empty())
        return flood_state;

    return filtered_state;
}

inline std::vector<ocean_flood_debug_point> build_ocean_flood_debug_points(const rocky_planet_profile& profile, size_t point_count = ocean_flood_debug_point_capacity, size_t sample_count = 1536u) {
    const auto basin_graph = build_ocean_basin_graph(1.0f, profile, sample_count);
    const auto flood_state = build_ocean_flood_state(basin_graph, 1.0f, 0.1f, 0.01f, profile.ocean_coverage);

    point_count = std::min(point_count, std::max<size_t>(1u, basin_graph.samples.size()));
    std::vector<ocean_flood_debug_point> points;
    points.reserve(point_count);

    for (size_t i = 0; i < point_count; ++i) {
        const size_t sample_index = std::min((i * basin_graph.samples.size()) / point_count, basin_graph.samples.size() - 1u);
        ocean_flood_debug_point point;
        point.normal = basin_graph.samples[sample_index].normal;
        point.region_index = flood_state.sample_region_indices[sample_index];
        points.push_back(point);
    }

    return points;
}

inline glm::vec3 fibonacci_direction(size_t index, size_t count) {
    const float sample_count = static_cast<float>(std::max<size_t>(count, 1u));
    const float t = (static_cast<float>(index) + 0.5f) / sample_count;
    const float y = 1.0f - 2.0f * t;
    const float radial = std::sqrt(glm::max(0.0f, 1.0f - y * y));
    constexpr float golden_angle = 2.39996323f;
    const float angle = golden_angle * static_cast<float>(index);
    return glm::vec3(std::cos(angle) * radial, y, std::sin(angle) * radial);
}

inline float seed_hash01(size_t seed) {
    unsigned int x = static_cast<unsigned int>(seed);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return static_cast<float>(x & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

inline glm::vec3 jitter_ocean_seed_normal(const glm::vec3& normal, float angular_jitter, size_t seed) {
    const glm::vec3 helper_axis = std::abs(normal.y) > 0.82f
        ? glm::vec3(1.f, 0.f, 0.f)
        : glm::vec3(0.f, 1.f, 0.f);
    const glm::vec3 tangent = glm::normalize(glm::cross(helper_axis, normal));
    const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

    glm::vec2 jitter(
        seed_hash01(seed * 2u + 1u) - 0.5f,
        seed_hash01(seed * 2u + 2u) - 0.5f);
    const float jitter_length = glm::length(jitter);
    if (jitter_length > 0.5f)
        jitter *= 0.5f / jitter_length;

    return glm::normalize(normal + (tangent * jitter.x + bitangent * jitter.y) * angular_jitter);
}

inline float ocean_seed_depth01(float floor_radius, float water_surface_radius, float shell_thickness) {
    return glm::clamp((water_surface_radius - floor_radius) / glm::max(shell_thickness, 0.0001f), 0.0f, 1.0f);
}

inline float resolved_ocean_shell_thickness(float shell_thickness, float particle_radius) {
    return glm::max(shell_thickness, glm::max(particle_radius * 4.0f, 0.0001f));
}

inline fluid_particle make_ocean_particle(const glm::vec3& normal, float radius, float depth01, float motion_scale) {
    fluid_particle particle;
    particle.position = glm::vec4(normal * radius, 1.0f);
    particle.predicted_position = particle.position;

    const glm::vec3 helper_axis = std::abs(normal.y) > 0.82f
        ? glm::vec3(1.f, 0.f, 0.f)
        : glm::vec3(0.f, 1.f, 0.f);
    const glm::vec3 tangent = glm::normalize(glm::cross(helper_axis, normal));
    const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
    const float swirl_a = std::sin(glm::dot(normal, glm::vec3(12.9898f, 78.233f, 37.719f)) * 4.0f);
    const float swirl_b = std::cos(glm::dot(normal, glm::vec3(39.3468f, 11.135f, 83.155f)) * 3.0f);
    glm::vec3 seed_direction = tangent * swirl_a + bitangent * swirl_b;
    if (glm::dot(seed_direction, seed_direction) > 0.000001f)
        seed_direction = glm::normalize(seed_direction);
    else
        seed_direction = tangent;

    const float tangential_speed = motion_scale * (0.004f + depth01 * 0.012f);
    particle.velocity = glm::vec4(seed_direction * tangential_speed, 0.0f);
    return particle;
}

inline ocean_seed_generation_result generate_ocean_seed_data(
    const ocean_seed_generation_params& params,
    const rocky_planet_profile& profile,
    const ocean_generation_progress_callback& progress_callback = {}) {
    ocean_seed_generation_result result;
    const auto report_progress = [&](float value) {
        if (progress_callback)
            progress_callback(glm::clamp(value, 0.0f, 1.0f));
    };

    report_progress(0.0f);

    const float coverage = params.coverage >= 0.0f
        ? glm::clamp(params.coverage, 0.0f, 1.0f)
        : glm::clamp(profile.ocean_coverage, 0.0f, 1.0f);

    result.resolved_shell_thickness = resolved_ocean_shell_thickness(params.shell_thickness, params.particle_radius);
    const size_t candidate_count = params.candidate_count > 0u
        ? params.candidate_count
        : std::max(params.target_particle_count * profile.ocean_candidate_multiplier, params.target_particle_count + 1u);
    result.basin_graph = build_ocean_basin_graph(params.base_radius, profile, candidate_count);
    report_progress(0.28f);
    result.flood_state = build_ocean_flood_state(
        result.basin_graph,
        params.base_radius,
        result.resolved_shell_thickness,
        params.particle_radius,
        coverage);
    if (params.primary_regions_only)
        result.flood_state = filter_to_primary_ocean_regions(result.basin_graph, result.flood_state, result.resolved_shell_thickness);
    report_progress(0.45f);

    result.water_surface_radius = result.flood_state.water_surface_radius;
    const float max_surface_radius = params.base_radius + result.resolved_shell_thickness;
    const double shell_capacity_volume_proxy = compute_shell_capacity_volume_proxy(result.basin_graph, max_surface_radius);
    const double flooded_volume_proxy = compute_ocean_volume_proxy(result.basin_graph, result.flood_state, max_surface_radius);
    result.flooded_volume_ratio = shell_capacity_volume_proxy > 0.0
        ? glm::clamp(static_cast<float>(flooded_volume_proxy / shell_capacity_volume_proxy), 0.0f, 1.0f)
        : 0.0f;
    result.effective_target_count = params.target_particle_count > 0u
        ? std::max<size_t>(256u, static_cast<size_t>(std::round(static_cast<double>(params.target_particle_count) * static_cast<double>(glm::clamp(result.flooded_volume_ratio, 0.02f, 1.0f)))))
        : 0u;
    result.particles.reserve(result.effective_target_count);
    const float angular_jitter = glm::clamp((params.particle_radius / glm::max(params.base_radius, 0.0001f)) * 1.35f, 0.00075f, 0.032f);
    report_progress(0.55f);

    const auto region_order = [&]() {
        std::vector<size_t> ordered(result.flood_state.regions.size());
        for (size_t i = 0; i < ordered.size(); ++i)
            ordered[i] = i;
        std::sort(ordered.begin(), ordered.end(), [&](size_t lhs, size_t rhs) {
            return result.flood_state.regions[lhs].minimum_floor_radius < result.flood_state.regions[rhs].minimum_floor_radius;
        });
        return ordered;
    }();

    size_t total_flooded_samples = 0u;
    for (const flooded_ocean_region& region : result.flood_state.regions)
        total_flooded_samples += region.sample_indices.size();

    std::vector<size_t> region_allocations(result.flood_state.regions.size(), 0u);
    size_t allocated_particles = 0u;
    if (total_flooded_samples > 0u) {
        for (const size_t region_index : region_order) {
            const size_t sample_count_in_region = result.flood_state.regions[region_index].sample_indices.size();
            const size_t allocation = std::min(
                sample_count_in_region,
                static_cast<size_t>(std::round(static_cast<double>(result.effective_target_count) * static_cast<double>(sample_count_in_region) / static_cast<double>(total_flooded_samples))));
            region_allocations[region_index] = allocation;
            allocated_particles += allocation;
        }

        for (const size_t region_index : region_order) {
            if (allocated_particles >= result.effective_target_count)
                break;
            if (region_allocations[region_index] == 0u && !result.flood_state.regions[region_index].sample_indices.empty()) {
                region_allocations[region_index] = 1u;
                ++allocated_particles;
            }
        }

        while (allocated_particles < result.effective_target_count) {
            bool added_any = false;
            for (const size_t region_index : region_order) {
                if (allocated_particles >= result.effective_target_count)
                    break;
                if (region_allocations[region_index] >= result.flood_state.regions[region_index].sample_indices.size())
                    continue;

                ++region_allocations[region_index];
                ++allocated_particles;
                added_any = true;
            }

            if (!added_any)
                break;
        }
    }

    report_progress(0.62f);

    const float weighted_sampling_start = 0.62f;
    const float weighted_sampling_end = 0.9f;
    for (const size_t region_index : region_order) {
        const auto& region_samples = result.flood_state.regions[region_index].sample_indices;
        const size_t allocation = std::min(region_allocations[region_index], region_samples.size());
        std::vector<float> cumulative_weights(region_samples.size(), 0.0f);
        float total_weight = 0.0f;
        for (size_t sample_slot = 0; sample_slot < region_samples.size(); ++sample_slot) {
            const ocean_fill_sample& sample = result.basin_graph.samples[region_samples[sample_slot]];
            const float depth01 = ocean_seed_depth01(sample.floor_radius, result.water_surface_radius, result.resolved_shell_thickness);
            const float weight = 0.08f + std::pow(depth01, 1.6f) * 3.4f;
            total_weight += weight;
            cumulative_weights[sample_slot] = total_weight;
        }

        for (size_t i = 0; i < allocation; ++i) {
            size_t selection_index = 0u;
            if (!region_samples.empty() && total_weight > 0.0f) {
                const float target = seed_hash01(region_index * 4099u + i * 17u + 31u) * total_weight;
                selection_index = static_cast<size_t>(std::lower_bound(cumulative_weights.begin(), cumulative_weights.end(), target) - cumulative_weights.begin());
                selection_index = std::min(selection_index, region_samples.size() - 1u);
            }
            const ocean_fill_sample& sample = result.basin_graph.samples[region_samples[selection_index]];
            const float depth01 = ocean_seed_depth01(sample.floor_radius, result.water_surface_radius, result.resolved_shell_thickness);
            const float shell_fill = glm::clamp(0.1f + depth01 * 0.22f + seed_hash01(region_index * 4099u + i * 17u) * 0.34f, 0.08f, 0.68f);
            const float radius = glm::mix(sample.floor_radius, result.water_surface_radius, shell_fill);
            const glm::vec3 seed_normal = jitter_ocean_seed_normal(sample.normal, angular_jitter, region_index * 8191u + i * 131u);
            result.particles.push_back(make_ocean_particle(seed_normal, radius, depth01, profile.ocean_motion_scale));

            if (result.effective_target_count > 0u) {
                const float region_fill_ratio = static_cast<float>(result.particles.size()) / static_cast<float>(result.effective_target_count);
                report_progress(glm::mix(weighted_sampling_start, weighted_sampling_end, glm::clamp(region_fill_ratio, 0.0f, 1.0f)));
            }
        }
    }

    const size_t fallback_count = result.effective_target_count > result.particles.size() ? result.effective_target_count - result.particles.size() : 0u;
    std::vector<size_t> flooded_sample_indices;
    flooded_sample_indices.reserve(total_flooded_samples);
    for (const flooded_ocean_region& region : result.flood_state.regions)
        flooded_sample_indices.insert(flooded_sample_indices.end(), region.sample_indices.begin(), region.sample_indices.end());

    for (size_t i = 0; i < fallback_count; ++i) {
        if (flooded_sample_indices.empty())
            break;

        const size_t selection_index = static_cast<size_t>(seed_hash01(i * 313u + result.particles.size() * 17u) * static_cast<float>(flooded_sample_indices.size()));
        const ocean_fill_sample& sample = result.basin_graph.samples[flooded_sample_indices[std::min(selection_index, flooded_sample_indices.size() - 1u)]];
        const float depth01 = ocean_seed_depth01(sample.floor_radius, result.water_surface_radius, result.resolved_shell_thickness);
        const float radius = glm::clamp(glm::mix(sample.floor_radius, result.water_surface_radius, 0.12f + seed_hash01(i * 911u + 5u) * 0.4f), sample.floor_radius, result.water_surface_radius);
        const glm::vec3 seed_normal = jitter_ocean_seed_normal(sample.normal, angular_jitter, i * 65537u + 29u);
        result.particles.push_back(make_ocean_particle(seed_normal, radius, depth01 * 0.85f, profile.ocean_motion_scale * 0.25f));

        if (fallback_count > 0u) {
            const float fallback_ratio = static_cast<float>(i + 1u) / static_cast<float>(fallback_count);
            report_progress(glm::mix(0.9f, 0.98f, glm::clamp(fallback_ratio, 0.0f, 1.0f)));
        }
    }

    report_progress(1.0f);
    return result;
}

inline std::vector<fluid_particle> create_ocean_seed_particles(size_t target_count, float base_radius, float shell_thickness, const rocky_planet_profile& profile, float particle_radius = 0.026f) {
    ocean_seed_generation_params params;
    params.target_particle_count = target_count;
    params.base_radius = base_radius;
    params.shell_thickness = shell_thickness;
    params.particle_radius = particle_radius;
    params.coverage = profile.ocean_coverage;
    return generate_ocean_seed_data(params, profile).particles;
}

inline void apply_ocean_flood_debug(shader& rocky_shader, const rocky_planet_profile& profile) {
    rocky_shader.use();
    rocky_shader.set_uni_int("floodDebugPointCount", 0);
}
}
