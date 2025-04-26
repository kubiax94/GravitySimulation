#pragma once
#include <unordered_map>
#include <GLFW/glfw3.h>

class input_system
{
public:

	//TODO: DODAC MYSZKE! NARAZIE TYLKO KLAWIATURA I POZYCJA MYSZKI
	static void update(GLFWwindow* window);

	static bool is_button_down(int key);

	static bool is_key_down(int key);
	static bool is_key_pressed(int key);
	static bool is_key_released(int key);

	static double get_mouse_x();
	static double get_mouse_move_x();
	static double get_mouse_y();
	static double get_mouse_move_y();

	static void on_key_press(int key);
	static void on_key_release(int key);

	static void on_mouse_button_press(int button);
	static void on_mouse_button_release(int button);

	static void on_mouse_move(const double x, const double y);

private:

	static std::unordered_map<int, bool> current_keys_;
	static std::unordered_map<int, bool> current_mouse_buttons_;
	static std::unordered_map<int, bool> previous_keys_;
	static double mouse_x_;
	static double mouse_y_;


};
