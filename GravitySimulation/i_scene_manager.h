#pragma once
#include "Component.h"

class component;

class i_scene_manager
{
public:
	virtual void register_in(component* comp) = 0;
	virtual void register_out(component* comp) = 0;
	virtual ~i_scene_manager() = default;
};
