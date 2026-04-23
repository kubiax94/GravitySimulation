#include "procedural_mesh_resource.h"

procedural_mesh_resource::procedural_mesh_resource(generator_fn generator, const std::string& name)
  : asset(asset_type::MESH, name), generator_(std::move(generator)) {
}

procedural_mesh_resource::procedural_mesh_resource(generator_with_progress_fn generator, const std::string& name)
    : asset(asset_type::MESH, name), generator_with_progress_(std::move(generator)) {
}

bool procedural_mesh_resource::load() {
    report_progress(0.02f);

    if (!generator_ && !generator_with_progress_)
        return false;

    if (generator_with_progress_) {
        generated_mesh_data_ = std::make_shared<MeshData>(generator_with_progress_([this](float progress) {
            report_progress(glm::mix(0.02f, 0.9f, glm::clamp(progress, 0.0f, 1.0f)));
        }));
    }
    else {
        generated_mesh_data_ = std::make_shared<MeshData>(generator_());
        report_progress(0.9f);
    }

    return generated_mesh_data_ && !generated_mesh_data_->vertecies.empty() && !generated_mesh_data_->indices.empty();
}

bool procedural_mesh_resource::finalize() {
    if (!generated_mesh_data_)
        return false;

    if (!mesh_)
        mesh_ = std::make_unique<Mesh>();

    report_progress(0.95f);
    mesh_->set_name(get_name());
    mesh_->set_mesh_data(generated_mesh_data_);
   const bool finalized = mesh_->finalize();
    report_progress(finalized ? 1.0f : 0.0f);
    return finalized;
}

void procedural_mesh_resource::unload() {
    if (mesh_)
        mesh_->cleanup();
    mesh_.reset();
    generated_mesh_data_.reset();
 asset::unload();
}

void procedural_mesh_resource::cleanup() {
    unload();
}

bool procedural_mesh_resource::is_vaild() {
    return mesh_ != nullptr && mesh_->is_vaild();
}

Mesh* procedural_mesh_resource::get_mesh() const {
    return mesh_.get();
}

std::shared_ptr<MeshData> procedural_mesh_resource::get_generated_mesh_data() const {
    return generated_mesh_data_;
}
