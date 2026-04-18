#pragma once
#include "async_base.h"

enum class resource_type
{
	UNKNOWN,
	MESH,
	SHADER,
	TEXTURE,
	FONT,
	AUDIO
};

enum class resource_status
{
	UNLOADED,
	LOADING,
	LOADED,
	FAILED
};

class resource : public async_base<bool>
{
	protected:
	resource_type type_;
	resource_status get_status_() const;
	std::string name_;
};

