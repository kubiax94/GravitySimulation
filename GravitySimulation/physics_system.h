#pragma once
#include "rigid_body.h"

class physics_system
{
	std::unordered_map<scene_node*, rigid_body*> entities_;
public:
	bool add(rigid_body* r_body);
	bool remove(rigid_body* rigid_body);
	
	void update(const float& dt);
};
