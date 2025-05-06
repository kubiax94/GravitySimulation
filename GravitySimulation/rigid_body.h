#pragma once

#include "Component.h"
#include "physics_data.h"


class rigid_body : public component, public physics_data
{
private:
	

public:
	static type_id_t type_id();
	rigid_body(scene_node* owner, const physics_data& p_data);
	rigid_body(scene_node* owner, const physics_data* p_data);

	void attach_to(scene_node* n_node) override;
	void apply_force(const glm::vec3& force);
	void clear_force();

	void apply_to_scene() const;

	void set_position(const glm::vec3& n_pos);
	void set_mass(const float& n_mass);

	glm::vec3 get_position() const;
	float get_mass() const;

	type_id_t get_type_id() const override;

	bool detach() override;

	void integrate(const float& dt);

	~rigid_body() override;
};
