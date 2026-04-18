#pragma once

#include "base_manager.h"
#include "Mesh.h"
#include "Shader.h"


class asset_manager : public base_manager<asset, uuid>
{
	std::unordered_map<uuid, std::unique_ptr<asset>> asset_list_;

public:
	void add_asset(std::unique_ptr<asset> n_asset);
	void remove_asset(const uuid& asset_id);
	  
	shader* create_shader(const std::string& name, const char* vertx_path, const char* frag_path);
	Mesh* create_mesh(MeshData& mesh_data);

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

