#pragma once

#include <glad/glad.h>

#include "Component.h"
#include "i_data_provider.h"
#include "physics_buffer.h"
#include "physics_data.h"
#include "uuid.h"


class rigid_body : public component, public i_data_provider
{
private:
	physics_data* p_data_;

	uuid compute_shader_id_;
	

public:
	static type_id_t type_id();
	rigid_body(scene_node* owner, physics_data& p_data);
	rigid_body(scene_node* owner, physics_data* p_data);

	void attach_to(scene_node* n_node) override;
	void apply_force(const glm::vec3& force);
	void clear_force();

	void apply_to_scene() const;

	void set_position(const glm::vec3& n_pos);
	void set_mass(const float& n_mass) const;

	glm::vec3 get_position() const;
	float get_mass() const;
	glm::vec3 get_velocity() const;

	type_id_t get_type_id() const override;

	physics_data* get_compute_data();
	uuid get_compute_shader_id() const;
	void set_compute_shader(const uuid& c_shader_uuid);

	bool detach() override;

	void integrate(const float& dt);

	~rigid_body() override;

	void* get_buffer_data() const override;
	size_t get_buffer_size() const override;
	size_t get_buffer_count() const override;
};
