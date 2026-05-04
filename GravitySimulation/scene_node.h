#pragma once

#include <iostream>
#include <cstdint>
#include <unordered_map>
#include <ranges>

#include "bounding_box.h"
#include "collision_layers.h"
#include "Transform.h"
#include "Component.h"

#include "i_scene_manager.h"
#include "uuid.h"

using type_id_t = std::size_t;

class component;
class i_scene_manager;

enum class search_options : uint8_t
{
	none = 0,
	include_self = 1 << 0,
	recursive_down = 1 << 1,
	search_up = 1 << 2,
	first = 1 << 3,
	all_node = search_up | recursive_down,
	all_node_first = first | all_node,
	all_node_self = include_self | all_node,
	all_node_self_first = include_self | first | all_node,
	parent_self = include_self | search_up,
	parent_self_first = include_self | first | search_up,
	child_self = include_self | recursive_down,
	child_self_first = include_self | first | recursive_down
};

inline search_options operator|(search_options lhs, search_options hhs) {
	return static_cast<search_options>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(hhs));
}
inline bool operator&(search_options lhs, search_options hhs) {
	return static_cast<bool>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(hhs));
}

class scene_node : public i_transformable
{
private:
	uuid id_;
	std::string name_;
	scene_node* parent_ = nullptr;

	std::unordered_map<std::string, scene_node*> children_;
	std::unordered_map<type_id_t, component*> components_;
 std::vector<component*> component_slots_;
 std::vector<type_id_t> component_association_type_ids_cache_;
	collision_mask_t collision_layer_mask_ = to_collision_mask(collision_layer::default_layer);
	collision_mask_t collision_query_mask_ = collision_mask_all;

	transform transform_;
	mutable bool dirty_transform_ = true;
	uint64_t transform_revision_ = 1;
	uint64_t orientation_revision_ = 1;
	mutable glm::mat4 global_matrix_model_ = transform_.get_local_model_matrix();

	i_scene_manager* scene_manager_;

	template <typename T = component>
	std::vector<T*>& find_component(const search_options& s_options, std::vector<T*>& result, scene_node* last);

	template <typename T = component>
	T* find_component_first(const search_options& s_options, scene_node* last);

	template <class T = scene_node>
	std::vector<T*> find_node(const std::string& node_name,
		search_options s_options, std::vector<T*>& result, const scene_node* last);

   void store_component(component* comp);
	void clear_component_slots(component* comp);
  void set_dirty(bool affect_non_translation = true);


public:
	scene_node(const std::string& name, scene_node* parent = nullptr, i_scene_manager* scene_manager = nullptr);
	scene_node(const std::string& name, scene_node* parent);

	void add_child(scene_node* n_node);
	void set_parent(scene_node* new_parent, bool keep_global_transform = true);
	[[nodiscard]] scene_node* get_parent() const;

	bool add_component(component* comp);

	i_scene_manager* get_scene_manager() const;

	template <typename T = component, typename... Args>
		requires std::is_constructible_v<T, Args...>
	T* add_component(Args&&... args);

	template <typename T = component>
	std::vector<T*> find_component(const search_options& s_options);

	template <typename T = component>
	T* find_component();

	template <class T = scene_node>
	std::vector<T*> find_node(const std::string& node_name, search_options s_options);

	template <typename T = scene_node, typename U = component>
	std::vector<T*> find_node_with(const std::string& node_name);

	const glm::vec3& get_position() const override;
	const glm::vec3& get_rotation() const override;
	const glm::vec3& get_scale() const override;

	template <typename T = component>
	bool has_component();

	template <typename T = component>
	T* try_get_component();

	template <typename T = component>
	const T* try_get_component() const;

	virtual void update();
	virtual void draw();
  virtual void on_collision_enter(const collision_event& event);
	virtual void on_collision_stay(const collision_event& event);
	virtual void on_collision_exit(const collision_event& event);
	[[nodiscard]] bounding_box get_subtree_world_bounding_box() const;
	[[nodiscard]] bounding_box get_local_subtree_bounding_box() const;

	const transform& get_transform() const;
	void set_transform(const transform& new_transform);

	glm::vec3 forward() const override;
	const glm::vec3& forward_local() const override;

	glm::vec3 right() const override;
	const glm::vec3& right_local() const override;

	glm::vec3 up() const override;
	const glm::vec3& up_local() const override;

	glm::vec3 get_global_position() const override;
	glm::vec3 get_global_rotation() override;
	glm::vec3 get_global_scale() const override;
	const glm::mat4& get_global_matrix_model() const override;

	void set_global_position(const glm::vec3& n_pos) override;

	void set_position(const glm::vec3& n_pos) override;
	void set_position(const float& x, const float& y, const float& z) override;

	void set_global_rotation(const glm::vec3& global_euler_deg) override;

	void set_rotation(const glm::vec3& n_rot) override;
	void set_rotation(const float& x, const float& y, const float& z) override;

	void set_global_scale(const glm::vec3& scalar) override;

	void set_scale(const float& x) override;
	void set_scale(const glm::vec3& n_sc) override;
	void set_scale(const float& x, const float& y, const float& z) override;

	void remove_component(component* component);
	void set_collision_layer(collision_layer layer);
	void set_collision_layer_mask(collision_mask_t layer_mask);
	[[nodiscard]] collision_mask_t get_collision_layer_mask() const;
	void set_collision_query_mask(collision_mask_t query_mask);
	[[nodiscard]] collision_mask_t get_collision_query_mask() const;

	uuid get_id() const;
	[[nodiscard]] uint64_t get_transform_revision() const;
  [[nodiscard]] uint64_t get_orientation_revision() const;

   ~scene_node() override;
	
};


template <typename T>
std::vector<T*>& scene_node::find_component(const search_options& s_options, std::vector<T*>& result, scene_node* last) {
	static_assert(std::is_base_of_v<component, T>, "T must derive from component");

 if (s_options & search_options::include_self)
	{
       if (auto* component = try_get_component<T>())
			result.push_back(component);

		if (s_options & search_options::first)
			return result;
	}

	for (const auto& child : children_ | std::views::values)
	{
		if (child == last)
			continue;

		if (s_options & search_options::recursive_down)
		{
            if (auto* component = child->try_get_component<T>()) {
				result.push_back(component);

				if (s_options & search_options::first)
					return result;
			}

			child->find_component<T>(search_options::recursive_down, result, this);
		}
	}

	if (s_options & search_options::search_up && parent_)
		parent_->find_component<T>(search_options::all_node_self, result, this);

	return result;
}

template <typename T>
T* scene_node::find_component_first(const search_options& s_options, scene_node* last) {
	static_assert(std::is_base_of_v<component, T>, "T must derive from component");

	if (s_options & search_options::include_self) {
		if (auto* component = try_get_component<T>())
			return component;
	}

	for (const auto& child : children_ | std::views::values)
	{
		if (child == last)
			continue;

		if (s_options & search_options::recursive_down) {
			if (auto* component = child->find_component_first<T>(search_options::child_self_first, this))
				return component;
		}
	}

	if (s_options & search_options::search_up && parent_)
		return parent_->find_component_first<T>(search_options::all_node_self_first, this);

	return nullptr;
}

template <class T>
std::vector<T*> scene_node::find_node(const std::string& node_name, search_options s_options,
	std::vector<T*>& result, const scene_node* last) {

	static_assert(std::is_base_of_v<scene_node, T>, "T must derived from scene_node");

	if (s_options & search_options::include_self && name_ == node_name)
	{
		result.push_back(static_cast<T*>(this));
		if (s_options & search_options::first)
			return result;
	}

	for (const auto& child : children_ | std::views::values)
	{
		if (child == last)
			continue;

		if (s_options & search_options::recursive_down) {
			if (child->name_ == node_name)
			{
				result.push_back(static_cast<T*>(child));

				if (s_options & search_options::first)
					return result;

			}
			if (!child->children_.empty())
				child->find_node<T>(node_name, search_options::recursive_down, result, this);
		}
	}

	if (s_options & search_options::search_up && parent_)
		parent_->find_node<T>(node_name, search_options::all_node_self, result, this);

	return result;

}

template <typename T, typename... Args>
	requires std::is_constructible_v<T, Args...>
T* scene_node::add_component(Args&&... args) {

	static_assert(std::is_base_of_v<component, T>, "T must derive from component");
	static_assert(std::is_constructible_v<T, Args...>, "T must derive from component");

	T* comp = new T(std::forward<Args>(args)...);
	comp->attach_to(this);
    store_component(comp);

	//scene_manager_->register_in(comp);

	return comp;
}

template <typename T>
std::vector<T*> scene_node::find_component(const search_options& s_options) {
	static_assert(std::is_base_of_v<component, T>, "T must derive from component");
	std::vector<T*> comps;

 if (s_options & search_options::include_self)
	{
        if (auto* component = try_get_component<T>())
			comps.push_back(component);

		if (s_options & search_options::first)
			return comps;
	}

	for (const auto& child : children_ | std::views::values)
	{

		if (s_options & search_options::recursive_down)
		{
            if (auto* component = child->try_get_component<T>()) {
				comps.push_back(component);

				if (s_options & search_options::first)
					return comps;
			}
			if (!child->children_.empty())
				child->find_component<T>(search_options::recursive_down, comps, this);
		}
	}

	if (s_options & search_options::search_up && parent_)
		parent_->find_component<T>(search_options::all_node_self, comps, this);

	return comps;
}

template <typename T>
T* scene_node::find_component() {
  return find_component_first<T>(search_options::all_node_self_first, nullptr);
}

template <typename T>
std::vector<T*> scene_node::find_node(const std::string& node_name, search_options s_options) {
	static_assert(std::is_base_of_v<scene_node, T>, "T must derived from scene_node");

	std::vector<scene_node*> nodes;

	if (s_options & search_options::include_self && name_ == node_name)
	{
		nodes.push_back(static_cast<T*>(this));
		if (s_options & search_options::first)
			return nodes;
	}

	for (const auto& child : children_ | std::views::values)
	{
		if (s_options & search_options::recursive_down) {
			if (child->name_ == node_name)
			{
				nodes.push_back(static_cast<T*>(child));

				if (s_options & search_options::first)
					return nodes;
			}

			if (!child->children_.empty())
				child->find_node<T>(node_name, search_options::recursive_down, nodes, this);
		}
	}

	if (s_options & search_options::search_up && parent_)
		parent_->find_node<T>(node_name, search_options::all_node_self, nodes, this);

	return nodes;

}

template <typename T>
bool scene_node::has_component() {
	static_assert(std::is_base_of_v<component, T>, "T must drive from component");
 return try_get_component<T>() != nullptr;

}

template <typename T>
T* scene_node::try_get_component() {
	static_assert(std::is_base_of_v<component, T>, "T must derive from component");
 const type_id_t type_id = get_type_id<T>();
	if (type_id >= component_slots_.size())
		return nullptr;

	return static_cast<T*>(component_slots_[type_id]);
}

template <typename T>
const T* scene_node::try_get_component() const {
	static_assert(std::is_base_of_v<component, T>, "T must derive from component");
 const type_id_t type_id = get_type_id<T>();
	if (type_id >= component_slots_.size())
		return nullptr;

	return static_cast<const T*>(component_slots_[type_id]);
}