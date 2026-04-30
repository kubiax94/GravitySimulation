#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "planet_terrain.h"
#include "planetary_water_domain_data.h"

class planetary_water_domain
{
    planetary_water_domain_desc desc_{};
    planetary_water_domain_textures textures_{};
    std::vector<unsigned char> physics_mask_data_;
    std::vector<unsigned char> render_mask_data_;
    std::vector<float> continuity_data_;
    std::vector<float> veto_data_;
    std::vector<float> water_level_data_;
    std::vector<unsigned short> region_id_data_;
    std::vector<float> shore_distance_data_;

    void release_textures();
    void upload_textures();

public:
    planetary_water_domain() = default;
    planetary_water_domain(const planetary_water_domain&) = delete;
    planetary_water_domain& operator=(const planetary_water_domain&) = delete;
    planetary_water_domain(planetary_water_domain&& other) noexcept;
    planetary_water_domain& operator=(planetary_water_domain&& other) noexcept;

    void clear();
    void rebuild(
        const planetary_water_domain_desc& desc,
        const planet_terrain::ocean_basin_graph& basin_graph,
        const planet_terrain::ocean_flood_state& flood_state);

    [[nodiscard]] const planetary_water_domain_desc& get_desc() const { return desc_; }
    [[nodiscard]] const planetary_water_domain_textures& get_textures() const { return textures_; }
    [[nodiscard]] const std::vector<unsigned char>& get_physics_mask_data() const { return physics_mask_data_; }
    [[nodiscard]] const std::vector<unsigned char>& get_render_mask_data() const { return render_mask_data_; }
    [[nodiscard]] const std::vector<float>& get_continuity_data() const { return continuity_data_; }
    [[nodiscard]] const std::vector<float>& get_veto_data() const { return veto_data_; }
    [[nodiscard]] const std::vector<float>& get_water_level_data() const { return water_level_data_; }
    [[nodiscard]] const std::vector<unsigned short>& get_region_id_data() const { return region_id_data_; }
    [[nodiscard]] const std::vector<float>& get_shore_distance_data() const { return shore_distance_data_; }

    ~planetary_water_domain();
};
