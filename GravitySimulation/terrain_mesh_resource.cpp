#include "terrain_mesh_resource.h"

terrain_mesh_resource::terrain_mesh_resource(
    const planet_terrain::rocky_planet_profile& profile,
    const planet_terrain::terrain_patch_generation_params& params,
    const std::string& name)
    : procedural_mesh_resource(
        [profile, params](const progress_fn& progress) {
            return planet_terrain::generate_terrain_patch_mesh(params, profile, progress);
        },
        name),
        profile_(profile),
        params_(params) {
}
