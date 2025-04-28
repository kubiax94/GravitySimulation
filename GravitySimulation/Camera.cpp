#include "Camera.h"

#include <glm/gtx/string_cast.hpp>

Camera::Camera(scene_node* owner, float yaw, float pitch) : transformable(owner), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY) {

	Yaw = yaw;
	Pitch = pitch;
	FOV = glm::radians(75.f);
}

glm::mat4 Camera::GetViewMatrix() {
	glm::vec3 scaled_pos = transform_->get_global_position() * visual_scale_;

	return glm::lookAt(scaled_pos,
				scaled_pos + transform_->forward(),
	                   transform_->up());
}
glm::mat4 Camera::GetProjectionMatrix(float aspectRatio)  {
	//if(dirtyProjection)
	return glm::perspective(FOV, aspectRatio, 0.1f, 10000.f);
}

void Camera::move(const glm::vec3& dir, const float& dt) {
	glm::vec3 c_pos = transform_->get_position();
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
	
	if (input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT))
	{
		auto mouse_move = input_system::get_mouse_move();
		Yaw -= mouse_move.x * 10.f * dt;
		Pitch -= mouse_move.y * 10.f * dt;

		if (Pitch > 89.0f)
			Pitch = 89.0f;

		if (Pitch < -89.0f)
			Pitch = -89.0f;

		glm::quat q_pitch = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.f, 0, 0));
		glm::quat q_yaw = glm::angleAxis(glm::radians(Yaw), glm::vec3(0, 1.0f, 0));

		glm::quat orient = q_yaw * q_pitch;

		glm::vec3 euler = glm::degrees(glm::eulerAngles(orient));
		transform_->set_rotation(euler);
	}

}

type_id_t Camera::get_type_id() const {
	return ::get_type_id<Camera>();
}

void Camera::RecalculateProjection() {
	projectionMatrix = glm::perspective(FOV, aspect_ratio_, 0.1f, 10000.f);
}

void Camera::set_postion(const glm::vec3& n_vec3) {
	transform_->set_position(n_vec3);
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
