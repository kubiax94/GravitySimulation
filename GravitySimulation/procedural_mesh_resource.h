#pragma once

#include <functional>
#include <memory>

#include "Mesh.h"

class procedural_mesh_resource : public asset
{
public:
 using progress_fn = std::function<void(float)>;
    using generator_fn = std::function<MeshData()>;
    using generator_with_progress_fn = std::function<MeshData(const progress_fn&)>;

private:
    generator_fn generator_;
    generator_with_progress_fn generator_with_progress_;
    std::shared_ptr<MeshData> generated_mesh_data_;
    std::unique_ptr<Mesh> mesh_;

public:
   explicit procedural_mesh_resource(generator_fn generator = {}, const std::string& name = "");
    explicit procedural_mesh_resource(generator_with_progress_fn generator, const std::string& name = "");

    bool load() override;
    bool finalize() override;
    void unload() override;
    void cleanup() override;
    bool is_vaild() override;

    Mesh* get_mesh() const;
    std::shared_ptr<MeshData> get_generated_mesh_data() const;
};
