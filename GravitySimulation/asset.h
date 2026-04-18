#pragma once
#include <string>

#include "uuid.h"

enum class asset_status : std::uint8_t
{
	LOADED,
	UNLOADED,
	FAILED
};

enum class asset_type : std::uint8_t
{
	SHADER,
	MODEL,
	MESH,
	LIGHTING
};

enum class shader_type : std::uint8_t
{
	unknown,
	visual_shader,
	compute_shader
};

class asset_manager;

class asset
{
	friend class asset_manager;

protected:

	uuid id_;
	std::string name_;
	asset_type type_;
	asset_status status_ = asset_status::UNLOADED;

public:
	asset(asset_type type, const std::string& name="");
	virtual ~asset() = default;

    // Provide a default constructor implementation in header for simple assets
    asset() = default;

	const uuid& get_id() const { return id_; }
	const std::string& get_name() const { return name_; }
	asset_type get_type() const { return type_; }

	void set_name(const std::string& name) { name_ = name; }

	virtual void cleanup() = 0;
	virtual bool is_vaild() = 0;

};

