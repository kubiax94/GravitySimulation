#include "planetary_water_domain.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace {
constexpr float pi = 3.14159265359f;
constexpr float two_pi = 6.28318530718f;
constexpr float half_pi = 1.57079632679f;

float smooth_disk_falloff(float normalized_distance_sq)
{
    if (normalized_distance_sq > 1.0f)
        return 0.0f;

    return 1.0f - normalized_distance_sq;
}
}

planetary_water_domain::planetary_water_domain(planetary_water_domain&& other) noexcept
    : desc_(other.desc_),
      textures_(other.textures_),
      physics_mask_data_(std::move(other.physics_mask_data_)),
      render_mask_data_(std::move(other.render_mask_data_)),
      continuity_data_(std::move(other.continuity_data_)),
      veto_data_(std::move(other.veto_data_)),
      water_level_data_(std::move(other.water_level_data_)),
      region_id_data_(std::move(other.region_id_data_)),
      shore_distance_data_(std::move(other.shore_distance_data_))
{
    other.textures_ = {};
    other.desc_ = {};
}

planetary_water_domain& planetary_water_domain::operator=(planetary_water_domain&& other) noexcept
{
    if (this != &other) {
        clear();
        desc_ = other.desc_;
        textures_ = other.textures_;
        physics_mask_data_ = std::move(other.physics_mask_data_);
        render_mask_data_ = std::move(other.render_mask_data_);
        continuity_data_ = std::move(other.continuity_data_);
        veto_data_ = std::move(other.veto_data_);
        water_level_data_ = std::move(other.water_level_data_);
        region_id_data_ = std::move(other.region_id_data_);
        shore_distance_data_ = std::move(other.shore_distance_data_);
        other.textures_ = {};
        other.desc_ = {};
    }

    return *this;
}

void planetary_water_domain::release_textures()
{
    if (textures_.physics_mask_texture != 0) {
        glDeleteTextures(1, &textures_.physics_mask_texture);
        textures_.physics_mask_texture = 0;
    }
    if (textures_.render_mask_texture != 0) {
        glDeleteTextures(1, &textures_.render_mask_texture);
        textures_.render_mask_texture = 0;
    }
    if (textures_.continuity_texture != 0) {
        glDeleteTextures(1, &textures_.continuity_texture);
        textures_.continuity_texture = 0;
    }
    if (textures_.veto_texture != 0) {
        glDeleteTextures(1, &textures_.veto_texture);
        textures_.veto_texture = 0;
    }
    if (textures_.water_level_texture != 0) {
        glDeleteTextures(1, &textures_.water_level_texture);
        textures_.water_level_texture = 0;
    }
    if (textures_.region_id_texture != 0) {
        glDeleteTextures(1, &textures_.region_id_texture);
        textures_.region_id_texture = 0;
    }
    if (textures_.shore_distance_texture != 0) {
        glDeleteTextures(1, &textures_.shore_distance_texture);
        textures_.shore_distance_texture = 0;
    }
}

void planetary_water_domain::upload_textures()
{
    const int width = std::max(desc_.width, 1);
    const int height = std::max(desc_.height, 1);

    if (!physics_mask_data_.empty()) {
        glGenTextures(1, &textures_.physics_mask_texture);
        if (textures_.physics_mask_texture != 0) {
            glBindTexture(GL_TEXTURE_2D, textures_.physics_mask_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, physics_mask_data_.data());
        }
    }

    if (!render_mask_data_.empty()) {
        glGenTextures(1, &textures_.render_mask_texture);
        if (textures_.render_mask_texture != 0) {
            glBindTexture(GL_TEXTURE_2D, textures_.render_mask_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, render_mask_data_.data());
        }
    }

    if (!continuity_data_.empty()) {
        glGenTextures(1, &textures_.continuity_texture);
        if (textures_.continuity_texture != 0) {
            glBindTexture(GL_TEXTURE_2D, textures_.continuity_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, continuity_data_.data());
        }
    }

    if (!veto_data_.empty()) {
        glGenTextures(1, &textures_.veto_texture);
        if (textures_.veto_texture != 0) {
            glBindTexture(GL_TEXTURE_2D, textures_.veto_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, veto_data_.data());
        }
    }

    if (!water_level_data_.empty()) {
        glGenTextures(1, &textures_.water_level_texture);
        if (textures_.water_level_texture != 0) {
            glBindTexture(GL_TEXTURE_2D, textures_.water_level_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, water_level_data_.data());
        }
    }

    if (!region_id_data_.empty()) {
        glGenTextures(1, &textures_.region_id_texture);
        if (textures_.region_id_texture != 0) {
            glBindTexture(GL_TEXTURE_2D, textures_.region_id_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, region_id_data_.data());
        }
    }

    if (!shore_distance_data_.empty()) {
        glGenTextures(1, &textures_.shore_distance_texture);
        if (textures_.shore_distance_texture != 0) {
            glBindTexture(GL_TEXTURE_2D, textures_.shore_distance_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, shore_distance_data_.data());
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void planetary_water_domain::clear()
{
    release_textures();
    physics_mask_data_.clear();
    render_mask_data_.clear();
    continuity_data_.clear();
    veto_data_.clear();
    water_level_data_.clear();
    region_id_data_.clear();
    shore_distance_data_.clear();
    desc_ = {};
}

void planetary_water_domain::rebuild(
    const planetary_water_domain_desc& desc,
    const planet_terrain::ocean_basin_graph& basin_graph,
    const planet_terrain::ocean_flood_state& flood_state)
{
    clear();
    desc_ = desc;

    const int width = std::max(desc_.width, 1);
    const int height = std::max(desc_.height, 1);
    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    physics_mask_data_.assign(pixel_count, 0u);
    render_mask_data_.assign(pixel_count, 0u);
    continuity_data_.assign(pixel_count, 0.0f);
    veto_data_.assign(pixel_count, 0.0f);
    water_level_data_.assign(pixel_count, 0.0f);
    region_id_data_.assign(pixel_count, 0u);
    shore_distance_data_.assign(pixel_count, 0.0f);

    if (basin_graph.samples.empty() || flood_state.sample_region_indices.empty()) {
        upload_textures();
        return;
    }

    std::vector<float> physics_mask(pixel_count, 0.0f);
    std::vector<float> coverage_mask(pixel_count, 0.0f);
    std::vector<float> continuity_mask(pixel_count, 0.0f);
    std::vector<float> veto_seed_mask(pixel_count, 0.0f);
    std::vector<float> next_mask(pixel_count, 0.0f);
    std::vector<float> next_physics_mask(pixel_count, 0.0f);
    std::vector<float> next_continuity_mask(pixel_count, 0.0f);
    std::vector<float> next_veto_seed_mask(pixel_count, 0.0f);
    std::vector<float> water_level_accum(pixel_count, 0.0f);
    std::vector<float> region_weight(pixel_count, 0.0f);

    for (size_t sample_index = 0; sample_index < basin_graph.samples.size(); ++sample_index) {
        const int region_index = flood_state.sample_region_indices[sample_index];
        if (region_index < 0)
            continue;

        const auto& sample = basin_graph.samples[sample_index];
        const glm::vec3& normal = sample.normal;
        const float latitude = std::asin(glm::clamp(normal.y, -1.0f, 1.0f));
        const float longitude = std::atan2(normal.z, normal.x);
        const float u = (longitude + pi) / two_pi;
        const float v = (latitude + half_pi) / pi;
        const int center_x = glm::clamp(static_cast<int>(u * static_cast<float>(width)), 0, width - 1);
        const int center_y = glm::clamp(static_cast<int>(v * static_cast<float>(height)), 0, height - 1);
        const float water_surface_radius = flood_state.regions[static_cast<size_t>(region_index)].water_surface_radius;
        const float normalized_water_level = glm::clamp(
            (water_surface_radius - desc_.planet_radius) / glm::max(desc_.shell_thickness, 0.0001f),
            0.0f,
            1.0f);
        const float depth01 = glm::clamp(
            (water_surface_radius - sample.floor_radius) / glm::max(desc_.shell_thickness, 0.0001f),
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
        const unsigned short encoded_region = static_cast<unsigned short>(glm::clamp(region_index + 1, 0, 65535));

        for (int offset_y = -physics_radius_y; offset_y <= physics_radius_y; ++offset_y) {
            const int y = center_y + offset_y;
            if (y < 0 || y >= height)
                continue;

            for (int offset_x = -physics_radius_x; offset_x <= physics_radius_x; ++offset_x) {
                const float normalized_offset_x = static_cast<float>(offset_x) / physics_texel_radius_x;
                const float normalized_offset_y = static_cast<float>(offset_y) / physics_texel_radius_y;
                const float distance_sq = normalized_offset_x * normalized_offset_x + normalized_offset_y * normalized_offset_y;
                const float falloff = smooth_disk_falloff(distance_sq);
                if (falloff <= 0.0f)
                    continue;

                const int wrapped_x = (center_x + offset_x + width) % width;
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(wrapped_x);
                const float weighted_strength = physics_strength * falloff;
                physics_mask[pixel_index] = glm::clamp(std::max(physics_mask[pixel_index], weighted_strength), 0.0f, 1.0f);
                water_level_accum[pixel_index] = std::max(water_level_accum[pixel_index], normalized_water_level);
                if (weighted_strength >= region_weight[pixel_index]) {
                    region_weight[pixel_index] = weighted_strength;
                    region_id_data_[pixel_index] = encoded_region;
                }
            }
        }

        for (int offset_y = -radius_y; offset_y <= radius_y; ++offset_y) {
            const int y = center_y + offset_y;
            if (y < 0 || y >= height)
                continue;

            for (int offset_x = -radius_x; offset_x <= radius_x; ++offset_x) {
                const float normalized_offset_x = static_cast<float>(offset_x) / texel_radius_x;
                const float normalized_offset_y = static_cast<float>(offset_y) / texel_radius_y;
                const float distance_sq = normalized_offset_x * normalized_offset_x + normalized_offset_y * normalized_offset_y;
                const float falloff = smooth_disk_falloff(distance_sq);
                if (falloff <= 0.0f)
                    continue;

                const int wrapped_x = (center_x + offset_x + width) % width;
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(wrapped_x);
                const float weighted_strength = sample_strength * falloff;
                coverage_mask[pixel_index] = glm::clamp(
                    coverage_mask[pixel_index] + weighted_strength,
                    0.0f,
                    1.0f);
                water_level_accum[pixel_index] = std::max(water_level_accum[pixel_index], normalized_water_level);
                if (weighted_strength >= region_weight[pixel_index]) {
                    region_weight[pixel_index] = weighted_strength;
                    region_id_data_[pixel_index] = encoded_region;
                }
            }
        }
    }

    auto sample_mask = [&](const std::vector<float>& mask, int x, int y) -> float {
        x = (x + width) % width;
        y = glm::clamp(y, 0, height - 1);
        return mask[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
    };

    for (int pass = 0; pass < 1; ++pass) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
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

                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const float center = physics_mask[pixel_index];
                const float averaged = weight_total > 0.0f ? neighbor_sum / weight_total : center;
                next_physics_mask[pixel_index] = glm::clamp(std::max(center, std::max(averaged * 0.92f, neighbor_max * 0.72f)), 0.0f, 1.0f);
            }
        }

        physics_mask.swap(next_physics_mask);
    }

    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
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

                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const float center = coverage_mask[pixel_index];
                const float smoothed = weight_total > 0.0f ? weighted_sum / weight_total : center;
                const float stitched = std::max(center, max_neighbor * 0.82f);
                next_mask[pixel_index] = glm::clamp(std::max(stitched, smoothed * 0.96f), 0.0f, 1.0f);
            }
        }

        coverage_mask.swap(next_mask);
    }

    const auto close_polar_cap_row = [&](int y) {
        const float latitude01 = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        const float latitude = latitude01 * pi - half_pi;
        const float latitude_cos = std::abs(std::cos(latitude));
        const float effective_longitudinal_samples = static_cast<float>(width) * latitude_cos;
        if (effective_longitudinal_samples > 8.0f)
            return;

        const float closure_blend = glm::clamp((8.0f - effective_longitudinal_samples) / 6.0f, 0.0f, 1.0f);
        const float full_cap_blend = 1.0f - glm::smoothstep(1.0f, 4.5f, effective_longitudinal_samples);
        float row_max_physics = 0.0f;
        float row_avg_physics = 0.0f;
        float row_max_coverage = 0.0f;
        float row_avg_coverage = 0.0f;
        float row_max_water_level = 0.0f;
        int active_pixels = 0;
        std::unordered_map<unsigned short, float> region_scores;

        for (int x = 0; x < width; ++x) {
            const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            const float physics_value = physics_mask[pixel_index];
            const float coverage_value = coverage_mask[pixel_index];
            row_max_physics = std::max(row_max_physics, physics_value);
            row_max_coverage = std::max(row_max_coverage, coverage_value);

            if (coverage_value > 0.001f || physics_value > 0.001f) {
                row_avg_physics += physics_value;
                row_avg_coverage += coverage_value;
                row_max_water_level = std::max(row_max_water_level, water_level_accum[pixel_index]);
                ++active_pixels;
                const unsigned short region_id = region_id_data_[pixel_index];
                if (region_id != 0u)
                    region_scores[region_id] += std::max(coverage_value, physics_value * 0.8f);
            }
        }

        if (active_pixels <= 0)
            return;

        row_avg_physics /= static_cast<float>(active_pixels);
        row_avg_coverage /= static_cast<float>(active_pixels);
        if (row_max_coverage <= 0.01f && row_max_physics <= 0.02f)
            return;

        const float active_ratio = static_cast<float>(active_pixels) / static_cast<float>(width);
        const float row_presence = glm::clamp(std::max(row_max_coverage, row_avg_coverage * 1.4f), 0.0f, 1.0f);
        const float cap_fill_strength = full_cap_blend * glm::smoothstep(0.02f, 0.10f, active_ratio) * glm::smoothstep(0.03f, 0.16f, row_presence);

        unsigned short dominant_region = 0u;
        float dominant_region_score = 0.0f;
        for (const auto& [region_id, score] : region_scores) {
            if (score > dominant_region_score) {
                dominant_region_score = score;
                dominant_region = region_id;
            }
        }

        const float representative_physics = glm::clamp(
            std::max(row_max_physics * glm::mix(0.82f, 1.0f, closure_blend), row_avg_physics * glm::mix(1.02f, 1.18f, closure_blend)),
            0.0f,
            1.0f);
        const float representative_coverage = glm::clamp(
            std::max(row_max_coverage * glm::mix(0.78f, 0.96f, closure_blend), row_avg_coverage * glm::mix(1.04f, 1.24f, closure_blend)),
            0.0f,
            1.0f);

        for (int x = 0; x < width; ++x) {
            const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            const float current_physics = physics_mask[pixel_index];
            const float current_coverage = coverage_mask[pixel_index];
            const float fill_gate = 1.0f - glm::smoothstep(0.10f, 0.38f, std::max(current_coverage, current_physics));
            const float fill_strength = std::max(closure_blend * fill_gate, cap_fill_strength);
            if (fill_strength <= 0.0f)
                continue;

            physics_mask[pixel_index] = glm::clamp(std::max(current_physics, representative_physics * fill_strength), 0.0f, 1.0f);
            coverage_mask[pixel_index] = glm::clamp(std::max(current_coverage, representative_coverage * fill_strength), 0.0f, 1.0f);
            if (coverage_mask[pixel_index] > 0.01f || physics_mask[pixel_index] > 0.01f) {
                water_level_accum[pixel_index] = std::max(water_level_accum[pixel_index], row_max_water_level);
                if (region_id_data_[pixel_index] == 0u)
                    region_id_data_[pixel_index] = dominant_region;
            }
        }
    };

    for (int y = 0; y < height; ++y)
        close_polar_cap_row(y);

    for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index)
        veto_seed_mask[pixel_index] = glm::clamp(std::max(coverage_mask[pixel_index] * 0.88f, physics_mask[pixel_index] * 0.96f), 0.0f, 1.0f);

    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const float center = veto_seed_mask[pixel_index];
                float orthogonalMin = center;
                float diagonalMin = center;
                float weightedSum = center * 0.36f;
                float weightTotal = 0.36f;

                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        if (offset_x == 0 && offset_y == 0)
                            continue;

                        const float value = sample_mask(veto_seed_mask, x + offset_x, y + offset_y);
                        const bool orthogonal = offset_x == 0 || offset_y == 0;
                        if (orthogonal)
                            orthogonalMin = std::min(orthogonalMin, value);
                        else
                            diagonalMin = std::min(diagonalMin, value);

                        const float kernel = orthogonal ? 0.12f : 0.05f;
                        weightedSum += value * kernel;
                        weightTotal += kernel;
                    }
                }

                const float smoothed = weightTotal > 0.0f ? weightedSum / weightTotal : center;
                const float conservativeBridge = std::max(center, orthogonalMin * 0.94f);
                const float thinLandPreserving = std::min(conservativeBridge, std::max(center, diagonalMin * 0.88f + orthogonalMin * 0.12f));
                next_veto_seed_mask[pixel_index] = glm::clamp(std::max(thinLandPreserving, smoothed * 0.92f), 0.0f, 1.0f);
            }
        }

        veto_seed_mask.swap(next_veto_seed_mask);
    }

    continuity_mask = coverage_mask;
    for (size_t pixel_index = 0; pixel_index < continuity_mask.size(); ++pixel_index)
        continuity_mask[pixel_index] = glm::clamp(std::max(continuity_mask[pixel_index], physics_mask[pixel_index] * 0.72f), 0.0f, 1.0f);

    for (int pass = 0; pass < 4; ++pass) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float weighted_sum = 0.0f;
                float weight_total = 0.0f;
                float neighbor_max = 0.0f;
                for (int offset_y = -2; offset_y <= 2; ++offset_y) {
                    for (int offset_x = -2; offset_x <= 2; ++offset_x) {
                        const int manhattan = std::abs(offset_x) + std::abs(offset_y);
                        float kernel = 0.0f;
                        if (offset_x == 0 && offset_y == 0)
                            kernel = 0.18f;
                        else if (manhattan == 1)
                            kernel = 0.10f;
                        else if (std::abs(offset_x) <= 1 && std::abs(offset_y) <= 1)
                            kernel = 0.06f;
                        else if (manhattan == 2)
                            kernel = 0.045f;
                        else
                            kernel = 0.025f;

                        const float value = sample_mask(continuity_mask, x + offset_x, y + offset_y);
                        weighted_sum += value * kernel;
                        weight_total += kernel;
                        neighbor_max = std::max(neighbor_max, value);
                    }
                }

                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const float center = continuity_mask[pixel_index];
                const float physics_anchor = physics_mask[pixel_index];
                const float water_level_anchor = water_level_accum[pixel_index];
                const float smoothed = weight_total > 0.0f ? weighted_sum / weight_total : center;
                const float anchored = std::max(center, physics_anchor * 0.76f + water_level_anchor * 0.12f);
                next_continuity_mask[pixel_index] = glm::clamp(std::max(anchored, std::max(smoothed * 0.985f, neighbor_max * 0.90f)), 0.0f, 1.0f);
            }
        }

        continuity_mask.swap(next_continuity_mask);
    }

    for (size_t pixel_index = 0; pixel_index < coverage_mask.size(); ++pixel_index) {
        const float physics_value = glm::smoothstep(0.42f, 0.74f, physics_mask[pixel_index]);
        const float value = glm::smoothstep(0.32f, 0.72f, coverage_mask[pixel_index]);
        const float continuity_value = glm::smoothstep(0.26f, 0.68f, continuity_mask[pixel_index]);
        const float veto_value = glm::smoothstep(0.30f, 0.70f, veto_seed_mask[pixel_index]);
        const size_t row_index = pixel_index / static_cast<size_t>(width);
        const float latitude01 = (static_cast<float>(row_index) + 0.5f) / static_cast<float>(height);
        const float latitude_abs = std::abs(latitude01 * 2.0f - 1.0f);
        const float polar_projection_compensation = glm::smoothstep(0.90f, 0.985f, latitude_abs);
        const float stable_value = glm::clamp(
            std::max(std::max(value, continuity_value), physics_value * glm::mix(0.0f, 0.20f, polar_projection_compensation)),
            0.0f,
            1.0f);
        physics_mask_data_[pixel_index] = static_cast<unsigned char>(glm::clamp(physics_value, 0.0f, 1.0f) * 255.0f);
        render_mask_data_[pixel_index] = static_cast<unsigned char>(stable_value * 255.0f);
        continuity_data_[pixel_index] = stable_value > 0.001f ? continuity_value : 0.0f;
        veto_data_[pixel_index] = stable_value > 0.001f ? veto_value : 0.0f;
        water_level_data_[pixel_index] = continuity_data_[pixel_index] > 0.01f ? water_level_accum[pixel_index] : 0.0f;
        if (continuity_data_[pixel_index] <= 0.01f || veto_data_[pixel_index] <= 0.01f)
            region_id_data_[pixel_index] = 0u;
    }

    const float reference_radius = glm::max(desc_.planet_radius + desc_.shell_thickness * 0.5f, 0.0001f);
    const float texel_scale_x = (two_pi * reference_radius) / static_cast<float>(width);
    const float texel_scale_y = (pi * reference_radius) / static_cast<float>(height);
    const float texel_scale = glm::max(std::min(texel_scale_x, texel_scale_y), 0.0001f);
    std::vector<float> distance_field(pixel_count, static_cast<float>(width + height));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            const bool is_water = render_mask_data_[pixel_index] > 8u;
            bool shoreline = !is_water;
            if (is_water) {
                for (int offset_y = -1; offset_y <= 1 && !shoreline; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        if (offset_x == 0 && offset_y == 0)
                            continue;
                        const int neighbor_x = (x + offset_x + width) % width;
                        const int neighbor_y = glm::clamp(y + offset_y, 0, height - 1);
                        const size_t neighbor_index = static_cast<size_t>(neighbor_y) * static_cast<size_t>(width) + static_cast<size_t>(neighbor_x);
                        if (render_mask_data_[neighbor_index] <= 8u) {
                            shoreline = true;
                            break;
                        }
                    }
                }
            }

            distance_field[pixel_index] = shoreline ? 0.0f : distance_field[pixel_index];
        }
    }

    for (int pass = 0; pass < width + height; ++pass) {
        bool changed = false;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const size_t pixel_index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                if (render_mask_data_[pixel_index] <= 8u)
                    continue;

                float best = distance_field[pixel_index];
                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        if (offset_x == 0 && offset_y == 0)
                            continue;
                        const int neighbor_x = (x + offset_x + width) % width;
                        const int neighbor_y = glm::clamp(y + offset_y, 0, height - 1);
                        const size_t neighbor_index = static_cast<size_t>(neighbor_y) * static_cast<size_t>(width) + static_cast<size_t>(neighbor_x);
                        const float step_cost = (offset_x == 0 || offset_y == 0) ? 1.0f : 1.41421356f;
                        best = std::min(best, distance_field[neighbor_index] + step_cost);
                    }
                }
                if (best + 0.0001f < distance_field[pixel_index]) {
                    distance_field[pixel_index] = best;
                    changed = true;
                }
            }
        }

        if (!changed)
            break;
    }

    for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        if (render_mask_data_[pixel_index] <= 8u) {
            shore_distance_data_[pixel_index] = 0.0f;
            region_id_data_[pixel_index] = 0u;
            continue;
        }

        shore_distance_data_[pixel_index] = distance_field[pixel_index] * texel_scale;
    }

    upload_textures();
}

planetary_water_domain::~planetary_water_domain()
{
    clear();
}
