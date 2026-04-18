#pragma once
#include <string>
#include <unordered_map>
#include <glad/glad.h>

class ssbo_manager
{
public:
	const static ssbo_manager& get_instance();

	template <typename T>
	void create_buffer(const std::string& buffer_name, const std::vector<T>& data);

	template<typename T>
	void update_buffer(const std::string& buffer_name, const std::vector<T>& data);

	template <typename T>
	void read_all_data(const std::string& buffer_name);

private:
	struct ssbo_info {
		GLuint id;
		size_t size;
	};

	template <typename T>
	struct ssbo_buffer
	{
		std::vector<T> data;
	};

	std::unordered_map<GLuint, ssbo_info*>  buffers_;

	const std::string& buffer_name_to_id();
};


template <typename T>
void ssbo_manager::create_buffer(const std::string& buffer_name, const std::vector<T>& data) {

	
	

}

