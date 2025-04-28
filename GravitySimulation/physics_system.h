#pragma once
#include "rigid_body.h"
#include "unit_system.h"

class physics_system
{
	std::unordered_map<scene_node*, rigid_body*> entities_;
	unit_system* u_sys;

public:
	physics_system(unit_system* u_sys);

	bool add(rigid_body* r_body);
	void gravity_simulation();
	;
	bool remove(rigid_body* rigid_body);

	void update(const float& dt);
};
