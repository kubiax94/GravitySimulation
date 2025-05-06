#pragma once
#include <unordered_map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>

class input_system
{
public:

	//TODO: DODAC MYSZKE! NARAZIE TYLKO KLAWIATURA I POZYCJA MYSZKI
	static void update(GLFWwindow* window);

	static bool is_button_down(int key);

	static bool is_key_down(int key);
	static bool is_key_pressed(int key);
	static bool is_key_released(int key);

	static const glm::vec3& get_mouse_pos();
	static const glm::vec3& get_mouse_move();
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

	static glm::vec3 mouse_pos_;
	static glm::vec3 mouse_previous_pos_;
	static glm::vec3 mouse_move_;
	static bool dirty_mouse_pos_;

	static void recalculate_move();

};
