#include "planetary_ocean_resource.h"

namespace {
    const std::vector<fluid_particle>& empty_particles() {
        static const std::vector<fluid_particle> particles;
        return particles;
    }
}

planetary_ocean_resource::planetary_ocean_resource(
    const planet_terrain::rocky_planet_profile& profile,
    const planet_terrain::ocean_seed_generation_params& params,
    const std::string& name)
    : asset(asset_type::MESH, name), profile_(profile), params_(params) {
}

bool planetary_ocean_resource::load() {
  report_progress(0.02f);
    result_ = std::make_shared<planet_terrain::ocean_seed_generation_result>(
       planet_terrain::generate_ocean_seed_data(params_, profile_, [this](float progress) {
            report_progress(glm::mix(0.02f, 0.92f, glm::clamp(progress, 0.0f, 1.0f)));
        }));
    report_progress(result_ ? 0.92f : 0.0f);
    return result_ != nullptr;
}

bool planetary_ocean_resource::finalize() {
    if (!result_)
        return false;

    status_ = asset_status::LOADED;
    report_progress(1.0f);
    return true;
}

void planetary_ocean_resource::unload() {
    result_.reset();
    asset::unload();
}

void planetary_ocean_resource::cleanup() {
    unload();
}

bool planetary_ocean_resource::is_vaild() {
    return result_ != nullptr && status_ == asset_status::LOADED;
}

const std::vector<fluid_particle>& planetary_ocean_resource::get_particles() const {
    if (!result_)
        return empty_particles();
    return result_->particles;
}
