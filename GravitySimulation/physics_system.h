#pragma once
#include "compute_shader.h"
#include "rigid_body.h"
#include "unit_system.h"
#include "uuid.h"
#include <glm/vec3.hpp>

class physics_system
{
	std::unordered_map<uuid, rigid_body*> entities_;
	std::vector<uuid> order_id_;
    std::unordered_map<uuid, size_t> order_index_;
	std::unordered_map<uuid, glm::vec3> previous_positions_;
	std::unordered_map<uuid, glm::vec3> current_positions_;

	std::unordered_map<uuid, compute_shader*> compute_shaders_;
	std::unordered_map<uuid, std::vector<rigid_body* >> compute_groups_;
	std::unordered_map<uuid, bool> readback_pending_;

	unit_system* u_sys_;
	static constexpr float simulation_speed_ = 20.0f;

	compute_shader* gravity_simulation_comp_;
 bool gpu_buffer_dirty_ = true;
 std::vector<physics_data> get_physics_data(const std::vector<rigid_body*>& bodies) const;
	void rebuild_order_indices();
 void sync_gpu_buffer(compute_shader* compute, const std::vector<rigid_body*>& bodies);
	float simulation_time_ = 0.f;

    // async readback control
    int readback_interval_ = 1;
    int frame_idx_ = 0;

public:
	physics_system(unit_system* u_sys);

	bool add(rigid_body* r_body);
	void compute_cpu();
 void compute_gpu(const float& dt);
	
	bool remove(rigid_body* rigid_body);

	void register_in(compute_shader* c_shader);
	void unregister(const uuid& c_uuid);
	void sync_scene_positions(float alpha) const;
	[[nodiscard]] size_t get_body_index(const uuid& id) const;
	[[nodiscard]] GLuint get_render_ssbo() const;

	void update(const float& dt);
};
