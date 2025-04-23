#include "transformable.h"

transformable::transformable(scene_node* node) : component(node), transform_(static_cast<i_transformable*>(node)) {
}

transformable::transformable(scene_node* node, i_transformable* transformable) : component(node),
transform_(transformable) {
}

type_id_t transformable::get_type_id() const {
	return ::get_type_id<transformable>();
}

const i_transformable* transformable::get_transform() const {
	return transform_;
}

void transformable::attach_to(scene_node* n_node) {
	component::attach_to(n_node);
}

bool transformable::detach() {
	return component::detach();
}