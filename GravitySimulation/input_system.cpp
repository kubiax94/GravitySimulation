#include "input_system.h"

std::unordered_map<int, bool> input_system::current_keys_;
std::unordered_map<int, bool> input_system::current_mouse_buttons_;
std::unordered_map<int, bool> input_system::previous_keys_;
double input_system::mouse_x_ = 0.0;
double input_system::mouse_y_ = 0.0;

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

double input_system::get_mouse_x() {
	return mouse_x_;
}

double input_system::get_mouse_y() {
	return  mouse_y_;
}

void input_system::on_key_press(int key) { current_keys_[key] = true; }

void input_system::on_key_release(int key) { current_keys_[key] = false; }

void input_system::on_mouse_button_press(int button) { current_mouse_buttons_[button] = true; }

void input_system::on_mouse_button_release(int button) { current_mouse_buttons_[button] = false; }

void input_system::on_mouse_move(const double x, const double y) { mouse_x_ = x; mouse_y_ = y; }
