#include "Transform.h"

const glm::vec3& transform::Up = glm::vec3(0.f, 1.f, 0.f);
const glm::vec3& transform::Forward = glm::vec3(0.f, 0.f, -1.f);
const glm::vec3& transform::Right = glm::vec3(1.f, 0.f, 0.f);

const glm::vec3& transform::forward() const {
	if (dirtyRotation)
		RecalculateRotation();

	return _forward;
}
const glm::vec3& transform::up() const {
	if (dirtyRotation)
		RecalculateRotation();

	return _up;
}
const glm::vec3& transform::right() const {
	if (dirtyRotation)
		RecalculateRotation();

	return _right;
}

const glm::vec3& transform::GetPosition() const {
	return position;
}
const glm::vec3& transform::GetRotation() const {
	return rotation;
}
const glm::vec3& transform::GetScale() const {
	return scale;
}
const glm::mat4& transform::get_local_model_matrix() const {
	if (dirtyModel) 
		RecalculateModel();

	return modelMatrix;
}
const glm::mat4& transform::GetRotationMatrix() const {
	if (dirtyRotation)
		RecalculateRotation();

	return rotationMatrix;
}
void transform::RecalculateRotation() const {
	rotationMatrix = glm::mat4(rotation_quat_);

	_forward = glm::normalize(-glm::vec3(rotationMatrix[2]));
	_right = glm::normalize(glm::vec3(rotationMatrix[0]));
	_up = glm::normalize(glm::vec3(rotationMatrix[1]));

	dirtyRotation = false;
}
void transform::RecalculateModel() const {

	if (dirtyRotation)
		RecalculateRotation();

	modelMatrix = glm::translate(glm::mat4(1.f), position)
		* rotationMatrix
		* glm::scale(glm::mat4(1.f), scale);

	dirtyModel = false;
}

void transform::LookAt(const glm::vec3& target, const glm::vec3& worldUp) {

	glm::vec3 f = glm::normalize(target - position);
	glm::vec3 r = glm::normalize(glm::cross(worldUp, f));
	glm::vec3 u = glm::cross(f, r);

	rotationMatrix = glm::mat4(1.f);
	rotationMatrix[0] = glm::vec4(r, 0.0f);
	rotationMatrix[1] = glm::vec4(u, 0.0f);
	rotationMatrix[2] = glm::vec4(f, 0.0f);

	dirtyRotation = false;
	dirtyModel = true;
}
void transform::Translate(const glm::vec3& offest) {
	position += offest;
	dirtyModel = true;
}
void transform::setPosition(const glm::vec3& nPos) {
	position = nPos;
	dirtyModel = true;
}
void transform::setRotation(const glm::vec3& nRot) {
	rotation = nRot;
	rotation_quat_ = glm::quat(glm::radians(nRot));
	dirtyRotation = true;
	dirtyModel = true;
}
void transform::setScale(const glm::vec3& nSc) {
	scale = nSc;
	dirtyModel = true;
}

void transform::set_rotation_quat(const glm::quat& quat) {
	rotation_quat_ = quat;
	rotation = glm::degrees(glm::eulerAngles(quat));
	dirtyRotation = true;
	dirtyModel = true;
}