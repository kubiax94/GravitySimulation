#pragma once

#include <cstdint>
#include <string>

class uuid
{
	uint64_t id_;
	static uint64_t generate();

public:
	uuid(const uint64_t& id);
	uuid();

	bool operator==(const uuid& other) const;
	bool operator!=(const uuid& other) const;
	operator uint64_t() const;

	std::string to_string() const;
};
