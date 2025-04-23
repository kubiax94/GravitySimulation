#include "Camera.h"

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
	std::cout << FOV;
	return glm::perspective(FOV, aspectRatio, 0.1f, 10000.f);
}

type_id_t Camera::get_type_id() const {
	return ::get_type_id<Camera>();
}

void Camera::RecalculateProjection() {
	projectionMatrix = glm::perspective(FOV, aspect_ratio_, 0.1f, 10000.f);
}

void Camera::RecalculateView() {
	viewMatrix = glm::lookAt(
		transform_->get_global_position(),
		transform_->get_global_position() + transform_->forward(),
		transform_->up()
	);

	dirty_view_ = false;
}
