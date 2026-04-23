#include "asset_manager.h"
#include <memory>

void asset_manager::add_asset(std::unique_ptr<asset> n_asset) {
	if (!n_asset || asset_list_.contains(n_asset->get_id()))
		return;
	auto const id = n_asset->get_id();
	asset_list_[id] = std::move(n_asset);
}

void asset_manager::remove_asset(const uuid& asset_id) {
   auto it = asset_list_.find(asset_id);
	if (it == asset_list_.end() || !it->second)
		return;

	it->second->cleanup();
	asset_list_.erase(it);
}

shader* asset_manager::create_shader(const std::string& name, const char* vertx_path, const char* frag_path) {
	auto n_shader = std::make_unique<shader>(vertx_path, frag_path);
	n_shader->set_name(name);
	shader* n_shader_ptr = n_shader.get();
	add_asset(std::move(n_shader));
	return n_shader_ptr;
}

compute_shader* asset_manager::create_compute_shader(const std::string& name, const char* compute_path) {
	auto n_shader = std::make_unique<compute_shader>(compute_path);
	n_shader->set_name(name);
	compute_shader* n_shader_ptr = n_shader.get();
	add_asset(std::move(n_shader));
	return n_shader_ptr;
}

texture* asset_manager::create_texture(const std::string& name, const std::string& texture_path) {
	auto n_texture = std::make_unique<texture>(texture_path, name);
	if (!n_texture->load())
		return nullptr;

	if (!n_texture->finalize()) {
		n_texture->unload();
		return nullptr;
	}

	texture* n_texture_ptr = n_texture.get();
	add_asset(std::move(n_texture));
	return n_texture_ptr;
}

Mesh* asset_manager::create_mesh(MeshData& mesh_data) {
	auto n_mesh = std::make_unique<Mesh>(mesh_data);
	Mesh* n_mesh_ptr = n_mesh.get();
	add_asset(std::move(n_mesh));
	return n_mesh_ptr;
}

planetary_ocean_resource* asset_manager::create_planetary_ocean_resource(
	const std::string& name,
	const planet_terrain::rocky_planet_profile& profile,
	const planet_terrain::ocean_seed_generation_params& params) {
	auto n_resource = std::make_unique<planetary_ocean_resource>(profile, params, name);
	planetary_ocean_resource* n_resource_ptr = n_resource.get();
	add_asset(std::move(n_resource));
	return n_resource_ptr;
}

terrain_mesh_resource* asset_manager::create_terrain_mesh_resource(
	const std::string& name,
	const planet_terrain::rocky_planet_profile& profile,
	const planet_terrain::terrain_patch_generation_params& params) {
	auto n_resource = std::make_unique<terrain_mesh_resource>(profile, params, name);
	terrain_mesh_resource* n_resource_ptr = n_resource.get();
	add_asset(std::move(n_resource));
	return n_resource_ptr;
}

procedural_mesh_resource* asset_manager::create_procedural_mesh_resource(const std::string& name, procedural_mesh_resource::generator_fn generator) {
	auto n_resource = std::make_unique<procedural_mesh_resource>(std::move(generator), name);
	procedural_mesh_resource* n_resource_ptr = n_resource.get();
	add_asset(std::move(n_resource));
	return n_resource_ptr;
}

procedural_mesh_resource* asset_manager::create_procedural_mesh_resource(const std::string& name, procedural_mesh_resource::generator_with_progress_fn generator) {
	auto n_resource = std::make_unique<procedural_mesh_resource>(std::move(generator), name);
	procedural_mesh_resource* n_resource_ptr = n_resource.get();
	add_asset(std::move(n_resource));
	return n_resource_ptr;
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
	