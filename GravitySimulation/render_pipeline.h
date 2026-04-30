#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "Renderer.h"
#include "instance_manager.h"
#include "texture.h"

class scene;
class compute_shader;

struct render_item {
    renderer* render = nullptr;
};

class render_pipeline
{
public:
    struct offscreen_attachment {
        GLenum attachment = GL_COLOR_ATTACHMENT0;
        GLuint texture = 0;
    };

    struct texture_binding {
        GLuint unit = 0;
        GLenum target = GL_TEXTURE_2D;
        GLuint texture = 0;
    };

    struct image_binding {
        GLuint unit = 0;
        GLuint texture = 0;
        GLint level = 0;
        GLboolean layered = GL_FALSE;
        GLint layer = 0;
        GLenum access = GL_READ_ONLY;
        GLenum format = GL_RGBA8;
    };

    struct compute_dispatch_desc {
        compute_shader& shader_program;
        glm::uvec3 groups = glm::uvec3(1u);
        std::vector<texture_binding> textures;
        std::vector<image_binding> images;
        std::function<void(compute_shader&)> pre_dispatch;
    };

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
    GLint offscreen_previous_framebuffer_ = 0;
    GLint offscreen_previous_viewport_[4] = { 0, 0, 0, 0 };
    bool offscreen_pass_active_ = false;

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
    bool begin_offscreen_pass(GLuint framebuffer, int width, int height, const std::vector<offscreen_attachment>& color_attachments, GLuint depth_texture = 0);
    void clear_offscreen_color(GLint draw_buffer_index, const float clear_color[4]) const;
    void set_offscreen_draw_attachments(const std::vector<GLenum>& draw_buffers) const;
    void bind_textures(const std::vector<texture_binding>& bindings) const;
    void unbind_textures(const std::vector<texture_binding>& bindings) const;
    void draw_fullscreen(shader& shader_program, Mesh& fullscreen_mesh, const std::function<void(shader&)>& pre_draw = nullptr) const;
    void dispatch_compute(const compute_dispatch_desc& dispatch_desc) const;
    void end_offscreen_pass();
    bool begin_particle_surface_input_pass(int width, int height);
    void end_particle_surface_input_pass();
    [[nodiscard]] const particle_surface_targets& get_particle_surface_targets() const { return particle_surface_targets_; }
    void release_scene_depth_texture();
    void release_particle_surface_targets();
    ~render_pipeline();
};