#pragma once
#include <vector>

#include "i_data_provider.h"

template<typename  T>
struct physics_buffer : i_data_provider
{
private:
	std::vector<T> buffers_data;
public:
	virtual ~physics_buffer();
	physics_buffer(std::vector<T>& d);
	void* get_buffer_data() const override;
	size_t get_buffer_size() const override;
	size_t get_buffer_count() const override;
};

template <typename T>
physics_buffer<T>::physics_buffer(std::vector<T>& d): buffers_data(d) {}

template <typename T>
void* physics_buffer<T>::get_buffer_data() const {
	return buffers_data.data();
}

template <typename T>
size_t physics_buffer<T>::get_buffer_size() const {
	return sizeof(T);
}

template <typename T>
size_t physics_buffer<T>::get_buffer_count() const {
	return buffers_data.size();
}

