#pragma once

#include "procedural_mesh_resource.h"
#include "planet_terrain.h"

class terrain_mesh_resource final : public procedural_mesh_resource
{
    planet_terrain::rocky_planet_profile profile_{};
    planet_terrain::terrain_patch_generation_params params_{};

public:
    explicit terrain_mesh_resource(
        const planet_terrain::rocky_planet_profile& profile = {},
        const planet_terrain::terrain_patch_generation_params& params = {},
        const std::string& name = "");

    const planet_terrain::rocky_planet_profile& get_profile() const { return profile_; }
    const planet_terrain::terrain_patch_generation_params& get_params() const { return params_; }
};
