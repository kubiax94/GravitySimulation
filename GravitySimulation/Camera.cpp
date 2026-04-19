#include "Camera.h"

#include <glm/gtx/string_cast.hpp>

Camera::Camera(scene_node* owner, float yaw, float pitch) : transformable(owner), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY) {

	Yaw = yaw;
	Pitch = pitch;
	FOV = glm::radians(75.f);
}

glm::mat4 Camera::GetViewMatrix() {
   const glm::vec3 position = transform_->get_global_position();

	return glm::lookAt(position,
				position + transform_->forward(),
	                   transform_->up());
}
glm::mat4 Camera::GetProjectionMatrix(float aspectRatio)  {
	//if(dirtyProjection)
   return glm::perspective(FOV, aspectRatio, near_plane_, far_plane_);
}

void Camera::sync_angles_to_transform() {
	const glm::vec3 direction = glm::normalize(transform_->forward());
   Yaw = glm::degrees(std::atan2(-direction.x, -direction.z));
	Pitch = glm::degrees(std::asin(glm::clamp(direction.y, -1.f, 1.f)));
}

void Camera::move(const glm::vec3& dir, const float& dt) {
   glm::vec3 c_pos = transform_->get_global_position();
   const bool sprinting = input_system::is_key_down(GLFW_KEY_LEFT_SHIFT)
		|| input_system::is_key_down(GLFW_KEY_RIGHT_SHIFT);
	const float speed_multiplier = sprinting ? SPRINT_MULTIPLIER : 1.0f;
	c_pos += dir * MovementSpeed * 8.0f * speed_multiplier * dt;

	set_postion(c_pos);
}

void Camera::process_input(const float& dt) {
  const bool right_mouse_down = input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT);
    const bool just_started_rotating = right_mouse_down && !rotating_with_mouse_;
	if (right_mouse_down && !rotating_with_mouse_) {
		sync_angles_to_transform();
		input_system::reset_mouse_delta();
	}
	rotating_with_mouse_ = right_mouse_down;

	if (input_system::is_key_down(GLFW_KEY_W))
		move(transform_->forward(), dt);

	if (input_system::is_key_down(GLFW_KEY_A))
		move(-transform_->right(), dt);

	if (input_system::is_key_down(GLFW_KEY_S))
		move(-transform_->forward(), dt);

	if (input_system::is_key_down(GLFW_KEY_D))
		move(transform_->right(), dt);

 if (right_mouse_down && !just_started_rotating)
	{
		auto mouse_move = input_system::get_mouse_move();
		Yaw -= mouse_move.x * MouseSensitivity;
		Pitch -= mouse_move.y * MouseSensitivity;

		if (Pitch > 89.0f)
			Pitch = 89.0f;

		if (Pitch < -89.0f)
			Pitch = -89.0f;

		glm::quat q_pitch = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.f, 0, 0));
		glm::quat q_yaw = glm::angleAxis(glm::radians(Yaw), glm::vec3(0, 1.0f, 0));

		glm::quat orient = q_yaw * q_pitch;

		glm::vec3 euler = glm::degrees(glm::eulerAngles(orient));
        transform_->set_global_rotation(euler);
	}

}

type_id_t Camera::get_type_id() const {
	return ::get_type_id<Camera>();
}

void Camera::RecalculateProjection() {
 projectionMatrix = glm::perspective(FOV, aspect_ratio_, near_plane_, far_plane_);
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
