#pragma once
#include <cstdint>
#include <string>

#include "async_base.h"

enum class resource_type : std::uint8_t
{
	UNKNOWN,
   SHADER,
	MODEL,
	MESH,
	TEXTURE,
	FONT,
   AUDIO,
	LIGHTING
};

enum class resource_status : std::uint8_t
{
	UNLOADED,
	LOADING,
	LOADED,
	FAILED
};

class resource : public async_base<bool>
{
    protected:
	resource_type type_ = resource_type::UNKNOWN;
	resource_status status_ = resource_status::UNLOADED;
	std::string name_;
	std::string source_path_;

    resource_status get_status_() const {
		return status_;
	}

	void set_status_(resource_status status) {
		status_ = status;
	}

 public:
	explicit resource(resource_type type = resource_type::UNKNOWN, const std::string& name = "")
		: type_(type), name_(name) {
	}

	virtual ~resource() = default;

   bool execute_task() override {
		set_status_(resource_status::LOADING);

		try {
			const bool loaded = load();
			set_status_(loaded ? resource_status::LOADED : resource_status::FAILED);
			return loaded;
		}
		catch (...) {
			set_status_(resource_status::FAILED);
			throw;
		}
	}

	virtual bool load() {
		return true;
	}

	virtual bool finalize() {
		return true;
	}

	virtual void unload() {
		set_status_(resource_status::UNLOADED);
	}

    resource_status get_resource_status() const {
		return get_status_();
	}

	resource_type get_resource_type() const {
		return type_;
	}

	const std::string& get_name() const {
		return name_;
	}

	void set_name(const std::string& name) {
		name_ = name;
	}

	const std::string& get_source_path() const {
		return source_path_;
	}

	void set_source_path(const std::string& source_path) {
		source_path_ = source_path;
	}
};

