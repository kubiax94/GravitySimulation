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
	explicit operator uint64_t() const;

	std::string to_string() const;
};

namespace std
{
	template<>
	struct hash<uuid>
	{
		std::size_t operator()(const uuid& u) const noexcept {
			return std::hash<uint64_t>()(static_cast<uint64_t>(u));
		}
	};

}