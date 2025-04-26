#pragma once
#ifndef TRANSFORM_H
#define TRANSFORM_H

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>


/**
 * Ogólne za³o¿enia, œwiêty obiekt, przechowuje tylko lokalne transformy i za nie odpowiada
 */
class transform
{
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 rotation = glm::vec3(0.f);
	glm::quat rotation_quat_ = glm::quat(1, 0, 0, 0);
	glm::vec3 scale = glm::vec3(1.f);

	mutable glm::vec3 _forward = Forward;
	mutable glm::vec3 _right = Right;
	mutable glm::vec3 _up = Up;

	mutable glm::mat4 rotationMatrix = glm::mat4(1.f);
	mutable glm::mat4 modelMatrix = glm::mat4(1.f);
	mutable bool dirtyModel = true;
	mutable bool dirtyRotation = true;

	void RecalculateModel() const;
	void RecalculateRotation() const;

public:
	static const glm::vec3& Forward;
	static const glm::vec3& Up;
	static const glm::vec3& Right;

	const glm::vec3& GetPosition() const;
	const glm::vec3& GetRotation() const;
	const glm::vec3& GetScale() const;

	const glm::mat4& get_local_model_matrix() const;
	const glm::mat4& GetRotationMatrix() const;

	void LookAt(const glm::vec3& target, const glm::vec3& worldUp);
	void Translate(const glm::vec3& offset);
	void setPosition(const glm::vec3& nPos);
	void setRotation(const glm::vec3& nRot);
	void set_rotation_quat(const glm::quat& quat);
	void setScale(const glm::vec3& nSc);

	const glm::vec3& forward() const;
	const glm::vec3& up() const;
	const glm::vec3& right() const;
};

class i_transformable
{
public:
	virtual const glm::vec3& forward() const = 0;
	virtual const glm::vec3& forward_local() const = 0;

	virtual const glm::vec3& right() const = 0;
	virtual const glm::vec3& right_local() const = 0;

	virtual const glm::vec3& up() const = 0;
	virtual const glm::vec3& up_local() const = 0;

	virtual const glm::vec3& get_position() const = 0;
	virtual glm::vec3 get_global_position() const = 0;

	virtual const glm::vec3& get_rotation() const = 0;
	virtual glm::vec3 get_global_rotation() = 0;

	virtual const glm::vec3& get_scale() const = 0;
	virtual const glm::vec3& get_global_scale() const = 0;

	virtual const glm::mat4& get_global_matrix_model() const = 0;

	virtual void set_global_position(const glm::vec3& n_pos) = 0;

	virtual void set_position(const glm::vec3& n_pos) = 0;
	virtual void set_position(const float& x, const float& y, const float& z) = 0;

	virtual void set_global_rotation(const glm::vec3& global_euler_deg) = 0;

	virtual void set_rotation(const glm::vec3& n_rot) = 0;
	virtual void set_rotation(const float& x, const float& y, const float& z) = 0;

	virtual void set_scale(const float& x) = 0;
	virtual void set_scale(const glm::vec3& n_sc) = 0;
	virtual void set_scale(const float& x, const float& y, const float& z) = 0;

	virtual ~i_transformable() = default;
};

#endif // !TRANSOFRM_H_
