#include "Component.h"


const bool component::is_attach() const
{
	return owner_node_ != nullptr;
}

scene_node* component::get_node() const {
	return owner_node_;
}

void component::attach_to(scene_node* n_node)
{
	owner_node_ = n_node;
}

bool component::detach()
{
	if (is_attach()) {
		owner_node_ = nullptr;
		return true;
	}

	return false;
}
