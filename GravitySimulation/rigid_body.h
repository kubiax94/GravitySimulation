#pragma once

#include "Component.h"

struct physics_data
{
	float mass = 1.f;
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 velocity = glm::vec3(0.f, 0.f, 0);
	glm::vec3 accumulated_force = glm::vec3(0.f);
};

class rigid_body : public component, public physics_data
{
private:
	

public:
	static type_id_t type_id();
	rigid_body(scene_node* owner, const physics_data& p_data);

	void attach_to(scene_node* n_node) override;
	void apply_force(const glm::vec3& force);
	void clear_force();

	type_id_t get_type_id() const override;

	bool detach() override;

	void integrate(const float& dt);

	~rigid_body() override;
};
