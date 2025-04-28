#pragma once
#ifndef SCENE_H
#define SCENE_H


#include <iostream>
#include <vector>
#include <unordered_map>
#include <ranges>

#include "Camera.h"
#include "Time.h"
#include "physics_system.h"


class scene : public i_scene_manager
{
	//TODO:
	//ObjectManager
	scene_node* root_;
	Camera* main_camera_;
	std::vector<Camera*> cameras_;
	sim::time* time_;

	unit_system* unit_sys_;
	physics_system physics_;
	
	

public:
	virtual ~scene();
	scene();
	scene(sim::time* time);

	void init();

	virtual void update();
	virtual void draw();

	void add_to_scene(scene_node* n_node) const;
	[[nodiscard]] scene_node* create_scene_node(const std::string& n_name);
	[[nodiscard]] scene_node* find_scene_node(const std::string& n_name) const;

	void register_in(component* comp) override;
	void register_out(component* comp) override;
};

#endif // !SCENE_H_
