#pragma once

#include <iostream>
#include <unordered_map>
#include <ranges>

#include "Transform.h"
#include "Component.h"

#include "i_scene_manager.h"

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
	std::string name_;
	scene_node* parent_ = nullptr;

	std::unordered_map<std::string, scene_node*> children_;
	std::unordered_map<type_id_t, component*> components_;

	transform transform_;
	mutable bool dirty_transform_ = true;
	mutable glm::mat4 global_matrix_model_ = transform_.get_local_model_matrix();

	i_scene_manager* scene_manager_;

	template <typename T = component>
	std::vector<T*>& find_component(const search_options& s_options, std::vector<T*>& result, scene_node* last);

	template <class T = scene_node>
	std::vector<T*> find_node(const std::string& node_name,
		search_options s_options, std::vector<T*>& result, const scene_node* last);

	void set_dirty();


public:
	scene_node(const std::string& name, scene_node* parent = nullptr, i_scene_manager* scene_manager = nullptr);
	scene_node(const std::string& name, scene_node* parent);

	void add_child(scene_node* n_node);

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

	virtual void update();
	virtual void draw();

	const transform& get_transform() const;
	void set_transform(const transform& new_transform);

	const glm::vec3& forward() const override;
	const glm::vec3& forward_local() const override;

	const glm::vec3& right() const override;
	const glm::vec3& right_local() const override;

	const glm::vec3& up() const override;
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

	~scene_node() override = default;
	
};


template <typename T>
std::vector<T*>& scene_node::find_component(const search_options& s_options, std::vector<T*>& result, scene_node* last) {
	static_assert(std::is_base_of_v<component, T>, "T must derive from component");

	if (s_options & search_options::include_self && has_component<T>())
	{
		result.push_back(static_cast<T*>(components_[get_type_id<T>()]));

		if (s_options & search_options::first)
			return result;
	}

	for (const auto& child : children_ | std::views::values)
	{
		if (child == last)
			continue;

		if (s_options & search_options::recursive_down)
		{
			if (child->has_component<T>()) {
				result.push_back(static_cast<T*>(child->components_[get_type_id<T>()]));

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
	components_[comp->get_type_id()] = comp;

	//scene_manager_->register_in(comp);

	return comp;
}

template <typename T>
std::vector<T*> scene_node::find_component(const search_options& s_options) {
	static_assert(std::is_base_of_v<component, T>, "T must derive from component");
	std::vector<T*> comps;

	if (s_options & search_options::include_self && has_component<T>())
	{
		comps.push_back(static_cast<T*>(components_[get_type_id<T>()]));

		if (s_options & search_options::first)
			return comps;
	}

	for (const auto& child : children_ | std::views::values)
	{

		if (s_options & search_options::recursive_down)
		{
			if (child->has_component<T>()) {
				comps.push_back(static_cast<T*>(child->components_[get_type_id<T>()]));

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
	std::vector<T*> comp = find_component<T>(search_options::all_node_self_first);

	return comp.empty() ? nullptr : comp[0];

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
	auto id = get_type_id<T>();

	return components_.contains(id);
}