#pragma once
#include "broadphase_pair.h"
#include "bounding_box.h"
#include "collision_broadphase.h"
#include "collision_data.h"
#include "collision_contact_data.h"
#include "collision_event.h"
#include "collision_narrowphase.h"
#include "collision_solver.h"
#include "compute_shader.h"
#include "contact_manifold.h"
#include "rigid_body.h"
#include "unit_system.h"
#include "uuid.h"
#include <cstdint>
#include <glm/vec3.hpp>
#include <unordered_map>
#include <unordered_set>

class collider;

enum class physics_gpu_stage : uint8_t
{
	gravity_integration = 0,
	collision_detection,
	collision_resolution,
	custom
};

struct collision_pair
{
	collider* first = nullptr;
	collider* second = nullptr;
	bounding_box overlap_bounds;

	[[nodiscard]] bool is_valid() const {
		return first != nullptr && second != nullptr && overlap_bounds.valid;
	}
};

struct solid_collision_contact
{
	collider* first = nullptr;
	collider* second = nullptr;
	bounding_box overlap_bounds;
	glm::vec3 normal = glm::vec3(0.0f);
	float penetration_depth = 0.0f;

	[[nodiscard]] bool is_valid() const {
		return first != nullptr && second != nullptr && overlap_bounds.valid && penetration_depth > 0.0f;
	}
};

struct collision_pair_key
{
	collider* first = nullptr;
	collider* second = nullptr;

	[[nodiscard]] bool operator==(const collision_pair_key& other) const {
		return first == other.first && second == other.second;
	}
};

struct collision_pair_key_hash
{
	[[nodiscard]] size_t operator()(const collision_pair_key& key) const {
		const auto first_hash = static_cast<size_t>(reinterpret_cast<std::uintptr_t>(key.first));
		const auto second_hash = static_cast<size_t>(reinterpret_cast<std::uintptr_t>(key.second));
		return first_hash ^ (second_hash << 1);
	}
};

class physics_system
{
	std::unordered_map<uuid, rigid_body*> entities_;
	std::vector<uuid> order_id_;
    std::unordered_map<uuid, size_t> order_index_;
	std::unordered_map<uuid, glm::vec3> previous_positions_;
	std::unordered_map<uuid, glm::vec3> current_positions_;

	std::unordered_map<uuid, compute_shader*> compute_shaders_;
	std::unordered_map<uuid, std::vector<rigid_body* >> compute_groups_;
   std::unordered_map<uuid, physics_gpu_stage> compute_shader_stages_;
	std::unordered_map<uuid, bool> readback_pending_;
	std::unordered_set<uuid> gpu_driven_nodes_;
  std::unordered_map<uuid, rigid_body*> rigid_bodies_by_node_id_;
  uuid default_compute_shader_id_;
  collision_broadphase collision_broadphase_;
  collision_narrowphase collision_narrowphase_;
  collision_solver collision_solver_;
	std::vector<collider*> colliders_;
   std::vector<broadphase_pair> collision_candidate_pairs_;
	std::vector<collision_pair> collision_pairs_;
    std::vector<contact_manifold> contact_manifolds_;
    std::unordered_map<contact_manifold_key, contact_manifold, contact_manifold_key_hash> contact_manifold_cache_;
    std::vector<solid_collision_contact> solid_collision_contacts_;
	std::unordered_map<collision_pair_key, collision_event, collision_pair_key_hash> previous_collision_events_;
	std::vector<collision_event> collision_enter_events_;
	std::vector<collision_event> collision_stay_events_;
	std::vector<collision_event> collision_exit_events_;

	unit_system* u_sys_;
   float simulation_speed_ = 20.0f;

	compute_shader* gravity_simulation_comp_;
 compute_shader* collision_detect_comp_ = nullptr;
 compute_shader* collision_resolve_comp_ = nullptr;
 bool gpu_buffer_dirty_ = true;
 std::vector<physics_data> get_physics_data(const std::vector<rigid_body*>& bodies) const;
   std::vector<collision_data> get_collision_data(const std::vector<rigid_body*>& bodies) const;
	void rebuild_order_indices();
 void sync_gpu_buffer(compute_shader* compute, const std::vector<rigid_body*>& bodies);
    void sync_collision_buffer(compute_shader* compute, const std::vector<rigid_body*>& bodies);
    void sync_collision_contact_buffer(compute_shader* compute, const std::vector<rigid_body*>& bodies);
 void apply_gpu_results_to_bodies(const std::vector<rigid_body*>& bodies, const std::vector<physics_data>& gpu_result);
	void sync_collision_contacts_from_gpu_data(const std::vector<collision_contact_data>& gpu_contacts, const std::vector<rigid_body*>& bodies);
    [[nodiscard]] bool requires_cpu_collision_stage(const std::vector<rigid_body*>& bodies) const;
 void run_default_gpu_pipeline(const float& dt);
	void run_custom_gpu_groups(const float& dt);
 void run_collision_stage_gpu(const std::vector<rigid_body*>& bodies);
	void run_collision_stage_cpu();
   static collision_pair_key make_collision_pair_key(collider* first, collider* second);
    static collision_event make_collision_event(collider* self, collider* other, const bounding_box& overlap_bounds);
 static solid_collision_contact make_solid_collision_contact(const contact_manifold& manifold);
    void sync_contact_manifold_cache();
 void update_contact_manifolds();
	void update_solid_collision_contacts();
	void update_collision_events();
  void dispatch_collision_events(const std::vector<collision_event>& events, void (scene_node::*handler)(const collision_event&));
	void remove_collision_state(collider* collider_component);
   void update_collision_pairs();
   int contact_solver_iterations_ = 8;
	float simulation_time_ = 0.f;

    // async readback control
    int readback_interval_ = 1;
    int frame_idx_ = 0;

public:
	physics_system(unit_system* u_sys);

	bool add(rigid_body* r_body);
 bool add(collider* collider_component);
	void compute_cpu();
 void compute_gpu(const float& dt);
	
	bool remove(rigid_body* rigid_body);
	bool remove(collider* collider_component);
	const std::vector<collider*>& get_colliders() const;
	const std::vector<collision_pair>& get_collision_pairs() const;
    const std::vector<contact_manifold>& get_contact_manifolds() const;
 const std::vector<solid_collision_contact>& get_solid_collision_contacts() const;
 [[nodiscard]] size_t get_cached_contact_manifold_count() const;
	[[nodiscard]] size_t get_persistent_contact_manifold_count() const;
	[[nodiscard]] size_t get_warm_contact_point_count() const;
	[[nodiscard]] uint32_t get_max_contact_persistence() const;
	const std::vector<collision_event>& get_collision_enter_events() const;
	const std::vector<collision_event>& get_collision_stay_events() const;
	const std::vector<collision_event>& get_collision_exit_events() const;

	void register_in(compute_shader* c_shader);
    void register_in(compute_shader* c_shader, physics_gpu_stage stage);
	void unregister(const uuid& c_uuid);
   void set_gpu_driven_nodes(const std::unordered_set<uuid>& node_ids);
   void set_simulation_speed(float speed);
	[[nodiscard]] float get_simulation_speed() const;
	void sync_scene_positions(float alpha) const;
	[[nodiscard]] size_t get_body_index(const uuid& id) const;
	[[nodiscard]] GLuint get_render_ssbo() const;

	void update(const float& dt);
};
