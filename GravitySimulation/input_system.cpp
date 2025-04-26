#include "input_system.h"

std::unordered_map<int, bool> input_system::current_keys_;
std::unordered_map<int, bool> input_system::current_mouse_buttons_;
std::unordered_map<int, bool> input_system::previous_keys_;

glm::vec3 input_system::mouse_pos_ = glm::vec3(0);
glm::vec3 input_system::mouse_previous_pos_ = glm::vec3(0);
glm::vec3 input_system::mouse_move_ = glm::vec3(0);
bool input_system::dirty_mouse_pos_ = false;

bool input_system::is_button_down(int key) {
	return current_mouse_buttons_[key];
}

bool input_system::is_key_down(int key) {
	return current_keys_[key];
}

bool input_system::is_key_pressed(int key) {
	return current_keys_[key] && !previous_keys_[key];
}

bool input_system::is_key_released(int key) {
	return !current_keys_[key] && previous_keys_[key];
}

const glm::vec3& input_system::get_mouse_pos() {
	return mouse_pos_;
}

const glm::vec3& input_system::get_mouse_move() {
	recalculate_move();
	return mouse_move_;
}

void input_system::on_key_press(int key) { current_keys_[key] = true; }

void input_system::on_key_release(int key) { current_keys_[key] = false; }

void input_system::on_mouse_button_press(int button) { current_mouse_buttons_[button] = true; }

void input_system::on_mouse_button_release(int button) { current_mouse_buttons_[button] = false; }

void input_system::on_mouse_move(const double x, const double y) {
	mouse_previous_pos_ = mouse_pos_;
	mouse_pos_ = glm::vec3(x, y, 0);

	dirty_mouse_pos_ = true;
}

void input_system::recalculate_move() {
	if (!dirty_mouse_pos_)
	{
		mouse_move_ = glm::vec3(0);
		return;
	}

	mouse_move_ = mouse_pos_ - mouse_previous_pos_;
	dirty_mouse_pos_ = false;
}
