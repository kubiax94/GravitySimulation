#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "Renderer.h"
#include "instance_manager.h"
#include "texture.h"

class scene;

struct render_item {
    renderer* render = nullptr;
};

class render_pipeline
{
public:
    struct particle_surface_targets {
        GLuint framebuffer = 0;
        GLuint blur_framebuffer = 0;
        GLuint coverage_texture = 0;
        GLuint coverage_ping_texture = 0;
        GLuint coverage_blur_texture = 0;
        GLuint depth_texture = 0;
        GLuint front_depth_texture = 0;
        GLuint front_depth_ping_texture = 0;
        GLuint front_depth_blur_texture = 0;
        int width = 0;
        int height = 0;
    };

    struct batch_key {
        shader* shader_ptr{};
        Mesh* mesh_ptr{};
        renderer_blend_mode blend_mode = renderer_blend_mode::opaque;
        renderer_cull_mode cull_mode = renderer_cull_mode::back;
        bool depth_write_enabled = true;

        bool operator==(const batch_key& other) const {
            return shader_ptr == other.shader_ptr
                && mesh_ptr == other.mesh_ptr
                && blend_mode == other.blend_mode
                && cull_mode == other.cull_mode
                && depth_write_enabled == other.depth_write_enabled;
        }
    };

    struct batch_key_hash {
        size_t operator()(const batch_key& key) const {
            return std::hash<void*>{}(key.shader_ptr)
                ^ (std::hash<void*>{}(key.mesh_ptr) << 1)
                ^ (static_cast<size_t>(key.blend_mode) << 2)
                ^ (static_cast<size_t>(key.cull_mode) << 3)
                ^ (static_cast<size_t>(key.depth_write_enabled) << 4);
        }
    };

    struct cached_batch {
        batch_key key;
        std::vector<renderer*> renders;
        bool uses_gpu_positions = false;
        std::vector<uint64_t> instance_revisions;
        std::vector<glm::mat4> instance_models;
      std::vector<int> instance_physics_indices;
    };

private:
    std::vector<render_item> items_;
    std::vector<renderer*> cached_submission_;
    std::vector<cached_batch> cached_batches_;
    instance_manager instance_manager_;
    texture scene_depth_texture_;
    particle_surface_targets particle_surface_targets_;
    GLint particle_surface_previous_framebuffer_ = 0;
    GLint particle_surface_previous_viewport_[4] = { 0, 0, 0, 0 };
    bool particle_surface_pass_active_ = false;

    [[nodiscard]] static bool is_render_valid(const renderer* render);
    void rebuild_cached_batches();
    [[nodiscard]] bool can_reuse_cached_batches() const;
    void update_cached_batch_instances(cached_batch& batch, bool use_gpu_positions) const;
    void ensure_scene_depth_texture(int width, int height);
    void ensure_particle_surface_targets(int width, int height);

public:
    void begin_frame();
    void submit(renderer* render);
    void submit(const render_item& item);
    void flush(Camera* camera, const scene* scene_context, const std::function<void(shader&)>& pre_draw = nullptr);
    void capture_scene_depth_texture(int width, int height);
    [[nodiscard]] GLuint get_scene_depth_texture_id() const { return scene_depth_texture_.get_id(); }
    bool begin_particle_surface_input_pass(int width, int height);
    void end_particle_surface_input_pass();
    [[nodiscard]] const particle_surface_targets& get_particle_surface_targets() const { return particle_surface_targets_; }
    void release_scene_depth_texture();
    void release_particle_surface_targets();
    ~render_pipeline();
};