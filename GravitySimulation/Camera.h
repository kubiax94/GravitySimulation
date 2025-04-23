#pragma once
#ifndef CAMERA_H_
#define CAMERA_H_

#include "transformable.h"

enum Camera_Movment {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

const float YAW = -90.f;
const float PITCH = 0.f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.f;


class Camera : public transformable
{
private:
	float aspect_ratio_ = 0.f;
	mutable bool dirty_projection_ = true;
	mutable bool dirty_view_ = true;

	glm::mat4 projectionMatrix = glm::mat4(0.f);
	glm::mat4 viewMatrix = glm::mat4(0.f);

	void RecalculateProjection();
	void RecalculateView();

public:

	float Yaw;
	float Pitch;

	float FOV;

	float MovementSpeed;
	float MouseSensitivity;
	float Zoom;

	Camera(scene_node* owner, float yaw = YAW, float pitch = PITCH);
	glm::mat4 GetViewMatrix();
	glm::mat4 GetProjectionMatrix(float aspectRatio);
	void UpdateAspectRatio();
	void LookAt(const transform& t);

	type_id_t get_type_id() const override;
};
#endif // !CAMERA_H_
