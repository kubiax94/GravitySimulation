#include "ssbo_manager.h"
#include <unordered_map>

const ssbo_manager& ssbo_manager::get_instance() {
	static const ssbo_manager instance;
	return  instance;
}

const std::string& ssbo_manager::buffer_name_to_id() {
	return "";
}
