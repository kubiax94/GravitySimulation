#include "uuid.h"

#include <random>
#include <sstream>

uint64_t uuid::generate() {
	static std::random_device rd;
	static std::mt19937_64 gen(rd());
	static std::uniform_int_distribution<uint64_t> dis;

	return dis(gen);
}

uuid::uuid(const uint64_t& id): id_(id) {}

uuid::uuid(): id_(generate()) {}

bool uuid::operator==(const uuid& other) const { return id_ == other.id_; }

bool uuid::operator!=(const uuid& other) const { return id_ != other.id_; }

uuid::operator unsigned long long() const { return id_; }

std::string uuid::to_string() const {
	std::stringstream ss;

	ss << std::hex << id_;
	return ss.str();
}
