#pragma once
#ifndef COMPONENT_H
#define COMPONENT_H

#include "scene_node.h"

using type_id_t = std::size_t;

inline type_id_t get_unique_type_id() {
	static type_id_t last_id = 0;
	return last_id++;
}

template <typename T>
type_id_t get_type_id() {
	static type_id_t id = get_unique_type_id();
	return id;
}

class scene_node;

class component
{
protected:
	scene_node* owner_node_;

public:
	virtual type_id_t get_type_id() const = 0;

	explicit component(scene_node* owner_node)
		: owner_node_(owner_node) {
	}

	const bool is_attach() const;

	template<typename T = component>
	T* get_as() const;

	scene_node* get_node() const;
	virtual void attach_to(scene_node* n_node);
	virtual bool detach();

	virtual ~component() = default;
};

template <typename T>
T* component::get_as() const {
	return dynamic_cast<T*>(this);
}

#endif
