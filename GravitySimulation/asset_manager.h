#pragma once

#include "base_manager.h"
#include "compute_shader.h"
#include "Mesh.h"
#include "planetary_ocean_resource.h"
#include "procedural_mesh_resource.h"
#include "Shader.h"
#include "terrain_mesh_resource.h"
#include "texture.h"


class asset_manager : public base_manager<asset, uuid>
{
	std::unordered_map<uuid, std::unique_ptr<asset>> asset_list_;

public:
	void add_asset(std::unique_ptr<asset> n_asset);
	void remove_asset(const uuid& asset_id);
	  
	shader* create_shader(const std::string& name, const char* vertx_path, const char* frag_path);
  compute_shader* create_compute_shader(const std::string& name, const char* compute_path);
	texture* create_texture(const std::string& name, const std::string& texture_path);
	Mesh* create_mesh(MeshData& mesh_data);
   planetary_ocean_resource* create_planetary_ocean_resource(
		const std::string& name,
		const planet_terrain::rocky_planet_profile& profile,
		const planet_terrain::ocean_seed_generation_params& params);
   terrain_mesh_resource* create_terrain_mesh_resource(
		const std::string& name,
		const planet_terrain::rocky_planet_profile& profile,
		const planet_terrain::terrain_patch_generation_params& params);
	procedural_mesh_resource* create_procedural_mesh_resource(const std::string& name, procedural_mesh_resource::generator_fn generator);
	procedural_mesh_resource* create_procedural_mesh_resource(const std::string& name, procedural_mesh_resource::generator_with_progress_fn generator);

	asset* find_asset_by_id(const uuid& id);
	std::vector<asset*> find_assets_by_type(const asset_type type);


	template<typename T>
	T* get_asset_type_as(const uuid id) {
		auto a = find_asset_by_id(id);
		if (!a)
			return nullptr;
		return dynamic_cast<T*>(a);
	}

	template<typename T>
	std::vector<T*> get_assets_of_type_as(const asset_type type) {
		auto assets = find_assets_by_type(type);
		std::vector<T*> casted_assets;
		for (auto& a : assets) {
			if (auto casted = dynamic_cast<T*>(a)) {
				casted_assets.push_back(casted);
			}
		}
		return casted_assets;
	}
};

