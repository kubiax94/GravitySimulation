#pragma once

#ifndef RENDERER_H
#define RENDERER_H

#include <functional>

#ifdef RENDERER_HEADLESS
#include "bounding_box.h"
#include "transformable.h"

class shader;
class Mesh;
class Camera;
#else
#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"
#include "bounding_box.h"
#endif

struct render_frame_context {
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 projection = glm::mat4(1.0f);
	glm::vec3 camera_position = glm::vec3(0.0f);
    unsigned int physics_ssbo = 0;
	int physics_body_index = -1;
	bool use_gpu_positions = false;
};

enum class renderer_blend_mode {
	opaque,
    alpha,
	additive
};

enum class renderer_cull_mode {
	back,
	front,
	none
};

class renderer : public transformable
{
private:
	shader* shader_;
	Mesh* mesh_;

	glm::vec3 visual_scale_ = glm::vec3(1.f);
	renderer_blend_mode blend_mode_ = renderer_blend_mode::opaque;
	renderer_cull_mode cull_mode_ = renderer_cull_mode::back;
	bool depth_write_enabled_ = true;
	bool gpu_driven_positions_ = false;
	int gpu_physics_index_ = -1;
	mutable bounding_box local_bounding_box_cache_;
	mutable bool local_bounding_box_cached_ = false;
	mutable bounding_box world_bounding_box_cache_;
	mutable uint64_t world_bounding_box_revision_ = 0;
	std::function<void(shader&)> material_pre_draw_;
	uint64_t material_batch_key_ = 0;

public:
	[[nodiscard]] type_id_t get_type_id() const override;
	float renderer_scale = 1.f;
	renderer(scene_node* s_node, shader* shader, Mesh* mesh);
	renderer(scene_node* s_node, shader* shader, Mesh* mesh, const float& v_scale);
	glm::mat4 get_visual_model_matrix() const;
    glm::mat4 get_visual_model_matrix_without_translation() const;
	void initialize();
	void set_visual_scale(const glm::vec3& scalar);
    void set_blend_mode(renderer_blend_mode blend_mode);
	[[nodiscard]] renderer_blend_mode get_blend_mode() const;
	void set_cull_mode(renderer_cull_mode cull_mode);
	[[nodiscard]] renderer_cull_mode get_cull_mode() const;
	void set_depth_write_enabled(bool enabled);
	[[nodiscard]] bool is_depth_write_enabled() const;
	void attach_to(scene_node* n_node) override;
	bool detach() override;
	void draw(Camera* c, const std::function<void(shader&)>& pre_draw = nullptr) const;
	void draw(const render_frame_context& frame_context, const std::function<void(shader&)>& pre_draw = nullptr) const;
	[[nodiscard]] uint64_t get_instance_revision(bool ignore_translation = false) const;
	void set_gpu_driven_positions(bool enabled);
	[[nodiscard]] bool uses_gpu_driven_positions() const;
	void set_gpu_physics_index(int index);
	[[nodiscard]] int get_gpu_physics_index() const;
    void set_material_pre_draw(std::function<void(shader&)> material_pre_draw, uint64_t material_batch_key = 0);
	void apply_material(shader& target_shader) const;
	[[nodiscard]] uint64_t get_material_batch_key() const;
  [[nodiscard]] bounding_box get_local_bounding_box() const;
	[[nodiscard]] bounding_box get_world_bounding_box() const;
	shader* get_shader() const { return shader_; }
	Mesh* get_mesh() const { return mesh_; }
};
#endif
