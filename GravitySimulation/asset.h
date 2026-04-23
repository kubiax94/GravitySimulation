#pragma once

#include "resource.h"
#include "uuid.h"

using asset_status = resource_status;
using asset_type = resource_type;

enum class shader_type : std::uint8_t
{
	unknown,
	visual_shader,
	compute_shader
};

class asset_manager;

class asset : public resource
{
	friend class asset_manager;

protected:

	uuid id_;

public:
	asset(asset_type type, const std::string& name="");
	virtual ~asset() = default;

 asset();

	const uuid& get_id() const { return id_; }
	asset_type get_type() const { return static_cast<asset_type>(get_resource_type()); }
	asset_status get_asset_status() const { return static_cast<asset_status>(get_resource_status()); }

	virtual void cleanup() = 0;
	virtual bool is_vaild() = 0;

};

