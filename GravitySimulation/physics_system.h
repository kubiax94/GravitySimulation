#pragma once
#include "compute_shader.h"
#include "rigid_body.h"
#include "unit_system.h"
#include "uuid.h"

class physics_system
{
	std::unordered_map<uuid, rigid_body*> entities_;
	std::vector<uuid> order_id_;

	unit_system* u_sys_;

	compute_shader* gravity_simulation_comp_;

	std::vector<physics_data> get_physics_data() const;

public:
	physics_system(unit_system* u_sys);

	bool add(rigid_body* r_body);
	void gravity_simulation_cpu();
	void gravity_simulation_gpu();
	
	bool remove(rigid_body* rigid_body);

	void update(const float& dt);
};
