#include "asset_manager.h"
#include <memory>

void asset_manager::add_asset(std::unique_ptr<asset> n_asset) {
	if (!n_asset || asset_list_.contains(n_asset->get_id()))
		return;
	auto const id = n_asset->get_id();
	asset_list_[id] = std::move(n_asset);
}

void asset_manager::remove_asset(const uuid& asset_id) {
	 auto found = find_asset_by_id(asset_id);
	 found->cleanup();
	 asset_list_.erase(found->get_id());
	 delete found;
}

shader* asset_manager::create_shader(const std::string& name, const char* vertx_path, const char* frag_path) {
	auto n_shader = std::make_unique<shader>(vertx_path, frag_path);
	n_shader->name_ = name;
	shader* n_shader_ptr = n_shader.get();
	add_asset(std::move(n_shader));
	return n_shader_ptr;
}

Mesh* asset_manager::create_mesh(MeshData& mesh_data) {
	auto n_mesh = std::make_unique<Mesh>(mesh_data);
	Mesh* n_mesh_ptr = n_mesh.get();
	add_asset(std::move(n_mesh));
	return n_mesh_ptr;
}

asset* asset_manager::find_asset_by_id(const uuid& id) {
	auto it = asset_list_.find(id);

	if (it == asset_list_.end())
		return nullptr;


	return  it->second.get();
}
std::vector<asset*> asset_manager::find_assets_by_type(const asset_type type) {
	std::vector<asset*> finds;

	for (const auto& [id, asset_ptr] : asset_list_) {
		if (asset_ptr->get_type() == type) {
			finds.push_back(asset_ptr.get());
		}
	}
	
	return finds;
}
	