#pragma once
#ifndef TRANSFORMABLE_COMP_H
#define TRANSFORMABLE_COMP_H

#include "Component.h"

class transformable : public component
{
protected:
	i_transformable* transform_;

public:
	explicit transformable(scene_node* node);

	explicit transformable(scene_node* node, i_transformable* transformable);

	type_id_t get_type_id() const override;
	virtual const i_transformable* get_transform() const;

	void attach_to(scene_node* n_node) override;
	bool detach() override;


	~transformable() override = default;

# endif 
};
