#pragma once
#ifndef SCENE_H
#define SCENE_H


#include <iostream>
#include <vector>
#include <unordered_map>
#include <ranges>
#include <algorithm>

#include "Camera.h"
#include "Time.h"
#include "asset_manager.h"
#include "physics_system.h"


class renderer;
class compute_shader;
class gpu_fluid_system_component;
class gpu_particle_system_component;

class scene : public i_scene_manager
{
	scene_node* root_;
	Camera* main_camera_;
	std::vector<Camera*> cameras_;
	std::vector<renderer*> renderers_;
   std::vector<gpu_fluid_system_component*> gpu_fluid_systems_;
   std::vector<gpu_particle_system_component*> gpu_particle_systems_;
	sim::time* time_;
	asset_manager asset_manager_;

	unit_system* unit_sys_;
	physics_system physics_;
	
	

public:
	virtual ~scene();
	scene();
	scene(sim::time* time);

	void init();

	virtual void update();
	virtual void draw();
	void sync_render() const;
  [[nodiscard]] Camera* get_main_camera() const { return main_camera_; }
  [[nodiscard]] scene_node* get_root_node() const { return root_; }
	[[nodiscard]] size_t get_renderer_physics_index(const renderer* render) const;
	[[nodiscard]] GLuint get_render_ssbo() const;
	const std::vector<renderer*>& get_renderers() const { return renderers_; }
   const std::vector<gpu_fluid_system_component*>& get_gpu_fluid_systems() const { return gpu_fluid_systems_; }
	const std::vector<gpu_particle_system_component*>& get_gpu_particle_systems() const { return gpu_particle_systems_; }
	unit_system* get_unit_system() const { return unit_sys_; }
	asset_manager& get_asset_manager() { return asset_manager_; }
	const asset_manager& get_asset_manager() const { return asset_manager_; }

	void add_to_scene(scene_node* n_node) const;
	[[nodiscard]] scene_node* create_scene_node(const std::string& n_name);
	[[nodiscard]] scene_node* find_scene_node(const std::string& n_name) const;

	void register_in(component* comp) override;
	void register_out(component* comp) override;
	void register_compute_shader(compute_shader* c_shader);
};

#endif // !SCENE_H_
