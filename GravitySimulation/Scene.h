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
#include "scene_loader.h"


class renderer;
class collider;
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
	scene_loader scene_loader_;

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
    [[nodiscard]] virtual bool has_primary_light() const { return false; }
	[[nodiscard]] virtual glm::vec3 get_primary_light_position() const { return glm::vec3(0.f); }
	[[nodiscard]] virtual glm::vec3 get_primary_light_color() const { return glm::vec3(1.f); }
	[[nodiscard]] virtual float get_primary_light_intensity() const { return 0.75f; }
  [[nodiscard]] Camera* get_main_camera() const { return main_camera_; }
  [[nodiscard]] scene_node* get_root_node() const { return root_; }
	[[nodiscard]] size_t get_renderer_physics_index(const renderer* render) const;
	[[nodiscard]] GLuint get_render_ssbo() const;
	const std::vector<renderer*>& get_renderers() const { return renderers_; }
   const std::vector<collider*>& get_colliders() const { return physics_.get_colliders(); }
 const std::vector<collision_pair>& get_collision_pairs() const { return physics_.get_collision_pairs(); }
   const std::vector<solid_collision_contact>& get_solid_collision_contacts() const { return physics_.get_solid_collision_contacts(); }
 const std::vector<collision_event>& get_collision_enter_events() const { return physics_.get_collision_enter_events(); }
	const std::vector<collision_event>& get_collision_stay_events() const { return physics_.get_collision_stay_events(); }
	const std::vector<collision_event>& get_collision_exit_events() const { return physics_.get_collision_exit_events(); }
   const std::vector<gpu_fluid_system_component*>& get_gpu_fluid_systems() const { return gpu_fluid_systems_; }
	const std::vector<gpu_particle_system_component*>& get_gpu_particle_systems() const { return gpu_particle_systems_; }
	unit_system* get_unit_system() const { return unit_sys_; }
   void set_simulation_speed(float speed) { physics_.set_simulation_speed(speed); }
	[[nodiscard]] float get_simulation_speed() const { return physics_.get_simulation_speed(); }
   void register_compute_shader(compute_shader* c_shader, physics_gpu_stage stage);
	asset_manager& get_asset_manager() { return asset_manager_; }
	const asset_manager& get_asset_manager() const { return asset_manager_; }
	scene_loader& get_scene_loader() { return scene_loader_; }
	const scene_loader& get_scene_loader() const { return scene_loader_; }

	void add_to_scene(scene_node* n_node) const;
	[[nodiscard]] scene_node* create_scene_node(const std::string& n_name);
	[[nodiscard]] scene_node* find_scene_node(const std::string& n_name) const;

	void register_in(component* comp) override;
	void register_out(component* comp) override;
	void register_compute_shader(compute_shader* c_shader);
};

#endif // !SCENE_H_
