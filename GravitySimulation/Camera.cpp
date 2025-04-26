#include "Camera.h"

#include <glm/gtx/string_cast.hpp>

Camera::Camera(scene_node* owner, float yaw, float pitch) : transformable(owner), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY) {

	Yaw = yaw;
	Pitch = pitch;
	FOV = glm::radians(75.f);
}

glm::mat4 Camera::GetViewMatrix() {
	
	return glm::lookAt(transform_->get_global_position(), transform_->get_global_position() + transform_->forward(), transform_->up());
}
glm::mat4 Camera::GetProjectionMatrix(float aspectRatio)  {
	//if(dirtyProjection)
	return glm::perspective(FOV, aspectRatio, 0.1f, 10000.f);
}

void Camera::move(const glm::vec3& dir, const float& dt) {
	glm::vec3 c_pos = transform_->get_global_position();
	c_pos += dir * 20.f * dt;

	set_postion(c_pos);
}

void Camera::process_input(const float& dt) {
	if (input_system::is_key_pressed(GLFW_KEY_W))
		move(transform_->forward(), dt);

	if (input_system::is_key_pressed(GLFW_KEY_A))
		move(-transform_->right(), dt);

	if (input_system::is_key_pressed(GLFW_KEY_S))
		move(-transform_->forward(), dt);

	if (input_system::is_key_pressed(GLFW_KEY_D))
		move(transform_->right(), dt);

	//std::cout << glm::to_string(input_system::get_mouse_move()) << std::endl;
	auto mouse_move = input_system::get_mouse_move();
	std::cout << input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT) << std::endl;
	if (input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT))
	{
		auto rot = transform_->get_global_rotation();
		rot.y -= mouse_move.x * 10.f * dt;
		rot.x -= mouse_move.y * 10.f * dt;
		rot.z = 0;

		if (rot.x > 89.0f)
			rot.x = 89.0f;

		if (rot.x < -89.0f)
			rot.x = -89.0f;

		transform_->set_global_rotation(rot);
	}

}

type_id_t Camera::get_type_id() const {
	return ::get_type_id<Camera>();
}

void Camera::RecalculateProjection() {
	projectionMatrix = glm::perspective(FOV, aspect_ratio_, 0.1f, 10000.f);
}

void Camera::set_postion(const glm::vec3& n_vec3) {
	transform_->set_global_position(n_vec3);
	dirty_projection_ = true;
}

void Camera::RecalculateView() {
	viewMatrix = glm::lookAt(
		transform_->get_global_position(),
		transform_->get_global_position() + transform_->forward(),
		transform_->up()
	);

	dirty_view_ = false;
}
