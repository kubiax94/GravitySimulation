#pragma once

#include <memory>
#include <vector>

#include "asset.h"
#include "fluid_particle.h"
#include "planet_terrain.h"

class planetary_ocean_resource : public asset
{
    planet_terrain::rocky_planet_profile profile_{};
    planet_terrain::ocean_seed_generation_params params_{};
    std::shared_ptr<planet_terrain::ocean_seed_generation_result> result_;

public:
    explicit planetary_ocean_resource(
        const planet_terrain::rocky_planet_profile& profile = {},
        const planet_terrain::ocean_seed_generation_params& params = {},
        const std::string& name = "");

    bool load() override;
    bool finalize() override;
    void unload() override;
    void cleanup() override;
    bool is_vaild() override;

    const planet_terrain::rocky_planet_profile& get_profile() const { return profile_; }
    const planet_terrain::ocean_seed_generation_params& get_params() const { return params_; }
    std::shared_ptr<planet_terrain::ocean_seed_generation_result> get_result() const { return result_; }
    const std::vector<fluid_particle>& get_particles() const;
};
