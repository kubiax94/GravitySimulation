#include "render_pipeline.h"

#ifdef RENDERER_HEADLESS
class frame_profiler {
public:
    struct scope_timer {};
    [[nodiscard]] static scope_timer measure_active(std::string) { return {}; }
};
#else
#include "frame_profiler.h"
#endif
#include "Camera.h"
#include "Shader.h"
#include "Mesh.h"
#include "spatial_query.h"

#ifdef RENDERER_HEADLESS
class scene {
public:
    [[nodiscard]] size_t get_renderer_physics_index(const renderer*) const { return static_cast<size_t>(-1); }
    [[nodiscard]] GLuint get_render_ssbo() const { return 0; }
};
#else
#include "Scene.h"
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <ranges>
#include <unordered_map>

namespace {
struct pipeline_render_state {
    renderer_blend_mode blend_mode = renderer_blend_mode::opaque;
    renderer_cull_mode cull_mode = renderer_cull_mode::back;
    bool depth_write_enabled = true;
};

void apply_pipeline_render_state(const pipeline_render_state& state) {
    if (state.blend_mode == renderer_blend_mode::additive) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    }
  else if (state.blend_mode == renderer_blend_mode::alpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else {
        glDisable(GL_BLEND);
    }

    glDepthMask(state.depth_write_enabled ? GL_TRUE : GL_FALSE);

    switch (state.cull_mode) {
    case renderer_cull_mode::none:
        glDisable(GL_CULL_FACE);
        break;
    case renderer_cull_mode::front:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    case renderer_cull_mode::back:
    default:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    }
}

pipeline_render_state make_pipeline_render_state(renderer_blend_mode blend_mode, renderer_cull_mode cull_mode, bool depth_write_enabled) {
    return pipeline_render_state{ blend_mode, cull_mode, depth_write_enabled };
}

render_frame_context build_frame_context(Camera* camera) {
    render_frame_context frame_context;
    if (!camera)
        return frame_context;

    int fbw = 1280;
    int fbh = 720;
    GLFWwindow* ctx = glfwGetCurrentContext();
    if (ctx)
        glfwGetFramebufferSize(ctx, &fbw, &fbh);

    const float aspect = (fbh == 0) ? 1.f : static_cast<float>(fbw) / static_cast<float>(fbh);
    frame_context.projection = camera->GetProjectionMatrix(aspect);
    frame_context.view = camera->GetViewMatrix();

    try {
        glm::mat4 invView = glm::inverse(frame_context.view);
        frame_context.camera_position = glm::vec3(invView[3]);
    }
    catch (...) {}

    return frame_context;
}
}

bool render_pipeline::is_render_valid(const renderer* render) {
    return render && render->get_shader() && render->get_mesh()
        && render->get_shader()->is_vaild() && render->get_mesh()->is_vaild();
}

void render_pipeline::rebuild_cached_batches() {
    std::unordered_map<batch_key, size_t, batch_key_hash> batch_indices;
    batch_indices.reserve(items_.size());

    cached_batches_.clear();
    cached_batches_.reserve(items_.size());

    cached_submission_.clear();
    cached_submission_.reserve(items_.size());

    for (const auto& item : items_) {
        cached_submission_.push_back(item.render);

        const batch_key key{
            item.render->get_shader(),
            item.render->get_mesh(),
            item.render->get_blend_mode(),
            item.render->get_cull_mode(),
            item.render->is_depth_write_enabled()
        };
        auto [it, inserted] = batch_indices.emplace(key, cached_batches_.size());
        if (inserted)
            cached_batches_.push_back({ key, {}, false, {}, {} });

        cached_batches_[it->second].renders.push_back(item.render);
    }
}

void render_pipeline::update_cached_batch_instances(cached_batch& batch, bool use_gpu_positions) const {
    const size_t render_count = batch.renders.size();
    if (render_count == 0)
        return;

    const bool requires_full_refresh = batch.uses_gpu_positions != use_gpu_positions
        || batch.instance_revisions.size() != render_count
        || batch.instance_models.size() != render_count;

    if (requires_full_refresh) {
        batch.uses_gpu_positions = use_gpu_positions;
        batch.instance_revisions.resize(render_count);
        batch.instance_models.resize(render_count);
       batch.instance_physics_indices.resize(render_count, -1);
    }

    for (size_t i = 0; i < render_count; ++i) {
        auto* render = batch.renders[i];
        batch.instance_physics_indices[i] = (use_gpu_positions && render) ? render->get_gpu_physics_index() : -1;
        const uint64_t revision = render ? render->get_instance_revision(use_gpu_positions) : 0;
        if (!requires_full_refresh && batch.instance_revisions[i] == revision)
            continue;

        batch.instance_revisions[i] = revision;
        batch.instance_models[i] = render
            ? (use_gpu_positions ? render->get_visual_model_matrix_without_translation() : render->get_visual_model_matrix())
            : glm::mat4(1.0f);
    }
}

bool render_pipeline::can_reuse_cached_batches() const {
    if (cached_submission_.size() != items_.size())
        return false;

    for (size_t i = 0; i < items_.size(); ++i) {
        if (cached_submission_[i] != items_[i].render)
            return false;
    }

    return true;
}

void render_pipeline::ensure_scene_depth_texture(int width, int height) {
    if (width <= 0 || height <= 0)
        return;

    if (scene_depth_texture_.get_width() == width
        && scene_depth_texture_.get_height() == height
        && scene_depth_texture_.is_vaild()) {
        return;
    }

    scene_depth_texture_.allocate_2d(
        width,
        height,
        GL_DEPTH_COMPONENT24,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr,
        GL_CLAMP_TO_EDGE,
        GL_CLAMP_TO_EDGE,
        GL_NEAREST,
        GL_NEAREST,
        false);
}

void render_pipeline::capture_scene_depth_texture(int width, int height) {
    ensure_scene_depth_texture(width, height);
    scene_depth_texture_.copy_from_framebuffer(0, 0, width, height);
}

bool render_pipeline::begin_offscreen_pass(GLuint framebuffer, int width, int height, const std::vector<offscreen_attachment>& color_attachments, GLuint depth_texture) {
    if (framebuffer == 0 || width <= 0 || height <= 0)
        return false;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &offscreen_previous_framebuffer_);
    glGetIntegerv(GL_VIEWPORT, offscreen_previous_viewport_);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    std::vector<GLenum> draw_buffers;
    draw_buffers.reserve(color_attachments.size());
    for (const auto& attachment : color_attachments) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment.attachment, GL_TEXTURE_2D, attachment.texture, 0);
        draw_buffers.push_back(attachment.attachment);
    }

    if (depth_texture != 0)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);

    if (!draw_buffers.empty())
        glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(offscreen_previous_framebuffer_));
        return false;
    }

    glViewport(0, 0, width, height);
    offscreen_pass_active_ = true;
    return true;
}

void render_pipeline::clear_offscreen_color(GLint draw_buffer_index, const float clear_color[4]) const {
    if (!offscreen_pass_active_)
        return;

    glClearBufferfv(GL_COLOR, draw_buffer_index, clear_color);
}

void render_pipeline::set_offscreen_draw_attachments(const std::vector<GLenum>& draw_buffers) const {
    if (!offscreen_pass_active_ || draw_buffers.empty())
        return;

    glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
}

void render_pipeline::set_offscreen_color_attachment(GLenum attachment, GLuint texture) const {
    if (!offscreen_pass_active_)
        return;

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, 0);
}

void render_pipeline::apply_render_state(const render_state_desc& state_desc) const {
    if (state_desc.depth_test_enabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glDepthFunc(state_desc.depth_func);
    glDepthMask(state_desc.depth_write_enabled ? GL_TRUE : GL_FALSE);

    if (state_desc.blend_enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(state_desc.blend_src, state_desc.blend_dst);
    }
    else {
        glDisable(GL_BLEND);
    }

    if (state_desc.cull_enabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(state_desc.cull_face);
    }
    else {
        glDisable(GL_CULL_FACE);
    }
}

void render_pipeline::bind_textures(const std::vector<texture_binding>& bindings) const {
    for (const auto& binding : bindings) {
        glActiveTexture(GL_TEXTURE0 + binding.unit);
        glBindTexture(binding.target, binding.texture);
    }

    if (!bindings.empty())
        glActiveTexture(GL_TEXTURE0);
}

void render_pipeline::unbind_textures(const std::vector<texture_binding>& bindings) const {
    for (const auto& binding : bindings) {
        glActiveTexture(GL_TEXTURE0 + binding.unit);
        glBindTexture(binding.target, 0);
    }

    if (!bindings.empty())
        glActiveTexture(GL_TEXTURE0);
}

void render_pipeline::draw_fullscreen(shader& shader_program, Mesh& fullscreen_mesh, const std::function<void(shader&)>& pre_draw) const {
    shader_program.use();
    if (pre_draw)
        pre_draw(shader_program);
    fullscreen_mesh.Draw();
}

void render_pipeline::draw_fullscreen_pass(const fullscreen_pass_desc& pass_desc) const {
    bind_textures(pass_desc.textures);
    draw_fullscreen(pass_desc.shader_program, pass_desc.fullscreen_mesh, pass_desc.pre_draw);
    unbind_textures(pass_desc.textures);
}

void render_pipeline::dispatch_compute(const compute_dispatch_desc& dispatch_desc) const {
    bind_textures(dispatch_desc.textures);

    for (const auto& image : dispatch_desc.images)
        glBindImageTexture(image.unit, image.texture, image.level, image.layered, image.layer, image.access, image.format);

    dispatch_desc.shader_program.use();
    if (dispatch_desc.pre_dispatch)
        dispatch_desc.pre_dispatch(dispatch_desc.shader_program);
    dispatch_desc.shader_program.dispatch(dispatch_desc.groups);

    for (const auto& image : dispatch_desc.images)
        glBindImageTexture(image.unit, 0, 0, GL_FALSE, 0, image.access, image.format);

    unbind_textures(dispatch_desc.textures);
}

void render_pipeline::copy_texture_2d(GLuint source_texture, GLuint destination_texture, int width, int height) const {
    if (source_texture == 0 || destination_texture == 0 || width <= 0 || height <= 0)
        return;

    glCopyImageSubData(source_texture, GL_TEXTURE_2D, 0, 0, 0, 0,
        destination_texture, GL_TEXTURE_2D, 0, 0, 0, 0,
        width, height, 1);
}

void render_pipeline::end_offscreen_pass() {
    if (!offscreen_pass_active_)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(offscreen_previous_framebuffer_));
    glViewport(
        offscreen_previous_viewport_[0],
        offscreen_previous_viewport_[1],
        offscreen_previous_viewport_[2],
        offscreen_previous_viewport_[3]);
    offscreen_pass_active_ = false;
}

void render_pipeline::ensure_particle_surface_targets(int width, int height) {
    if (width <= 0 || height <= 0)
        return;

    if (particle_surface_targets_.framebuffer == 0)
        glGenFramebuffers(1, &particle_surface_targets_.framebuffer);
    if (particle_surface_targets_.blur_framebuffer == 0)
        glGenFramebuffers(1, &particle_surface_targets_.blur_framebuffer);
    if (particle_surface_targets_.coverage_texture == 0)
        glGenTextures(1, &particle_surface_targets_.coverage_texture);
    if (particle_surface_targets_.coverage_ping_texture == 0)
        glGenTextures(1, &particle_surface_targets_.coverage_ping_texture);
    if (particle_surface_targets_.coverage_blur_texture == 0)
        glGenTextures(1, &particle_surface_targets_.coverage_blur_texture);
    if (particle_surface_targets_.depth_texture == 0)
        glGenTextures(1, &particle_surface_targets_.depth_texture);
    if (particle_surface_targets_.front_depth_texture == 0)
        glGenTextures(1, &particle_surface_targets_.front_depth_texture);
    if (particle_surface_targets_.front_depth_ping_texture == 0)
        glGenTextures(1, &particle_surface_targets_.front_depth_ping_texture);
    if (particle_surface_targets_.front_depth_blur_texture == 0)
        glGenTextures(1, &particle_surface_targets_.front_depth_blur_texture);

    if (particle_surface_targets_.width == width && particle_surface_targets_.height == height)
        return;

    glBindTexture(GL_TEXTURE_2D, particle_surface_targets_.coverage_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, particle_surface_targets_.coverage_ping_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, particle_surface_targets_.coverage_blur_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, particle_surface_targets_.depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, particle_surface_targets_.front_depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, particle_surface_targets_.front_depth_ping_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, particle_surface_targets_.front_depth_blur_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    particle_surface_targets_.width = width;
    particle_surface_targets_.height = height;
}

bool render_pipeline::begin_particle_surface_input_pass(int width, int height) {
    ensure_particle_surface_targets(width, height);
    if (particle_surface_targets_.framebuffer == 0
        || particle_surface_targets_.coverage_texture == 0
        || particle_surface_targets_.depth_texture == 0)
        return false;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &particle_surface_previous_framebuffer_);
    glGetIntegerv(GL_VIEWPORT, particle_surface_previous_viewport_);

    glBindFramebuffer(GL_FRAMEBUFFER, particle_surface_targets_.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, particle_surface_targets_.coverage_texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, particle_surface_targets_.front_depth_texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, particle_surface_targets_.depth_texture, 0);

    const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, draw_buffers);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, particle_surface_previous_framebuffer_);
        return false;
    }

    glViewport(0, 0, width, height);
    const float coverage_clear[4] = { 0.f, 0.f, 0.f, 0.f };
    const float front_depth_clear[4] = { 1.f, 0.f, 0.f, 1.f };
    glClearBufferfv(GL_COLOR, 0, coverage_clear);
    glClearBufferfv(GL_COLOR, 1, front_depth_clear);
    glClear(GL_DEPTH_BUFFER_BIT);
    particle_surface_pass_active_ = true;
    return true;
}

void render_pipeline::end_particle_surface_input_pass() {
    if (!particle_surface_pass_active_)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(particle_surface_previous_framebuffer_));
    glViewport(
        particle_surface_previous_viewport_[0],
        particle_surface_previous_viewport_[1],
        particle_surface_previous_viewport_[2],
        particle_surface_previous_viewport_[3]);
    particle_surface_pass_active_ = false;
}

void render_pipeline::release_particle_surface_targets() {
    if (particle_surface_targets_.framebuffer != 0)
        glDeleteFramebuffers(1, &particle_surface_targets_.framebuffer);
    if (particle_surface_targets_.blur_framebuffer != 0)
        glDeleteFramebuffers(1, &particle_surface_targets_.blur_framebuffer);
    if (particle_surface_targets_.coverage_texture != 0)
        glDeleteTextures(1, &particle_surface_targets_.coverage_texture);
    if (particle_surface_targets_.coverage_ping_texture != 0)
        glDeleteTextures(1, &particle_surface_targets_.coverage_ping_texture);
    if (particle_surface_targets_.coverage_blur_texture != 0)
        glDeleteTextures(1, &particle_surface_targets_.coverage_blur_texture);
    if (particle_surface_targets_.depth_texture != 0)
        glDeleteTextures(1, &particle_surface_targets_.depth_texture);
    if (particle_surface_targets_.front_depth_texture != 0)
        glDeleteTextures(1, &particle_surface_targets_.front_depth_texture);
    if (particle_surface_targets_.front_depth_ping_texture != 0)
        glDeleteTextures(1, &particle_surface_targets_.front_depth_ping_texture);
    if (particle_surface_targets_.front_depth_blur_texture != 0)
        glDeleteTextures(1, &particle_surface_targets_.front_depth_blur_texture);

    particle_surface_targets_ = {};
    particle_surface_pass_active_ = false;
}

render_pipeline::~render_pipeline() {
    release_scene_depth_texture();
    release_particle_surface_targets();
}

void render_pipeline::begin_frame() {
    items_.clear();
}

void render_pipeline::release_scene_depth_texture() {
    scene_depth_texture_.cleanup();
}

void render_pipeline::submit(renderer* render) {
    if (!is_render_valid(render))
        return;

    items_.push_back({ render });
}

void render_pipeline::submit(const render_item& item) {
    if (!is_render_valid(item.render))
        return;

    items_.push_back(item);
}

void render_pipeline::flush(Camera* camera, const scene* scene_context, const std::function<void(shader&)>& pre_draw) {
    if (!camera)
        return;

    const render_frame_context frame_context = build_frame_context(camera);
   const view_frustum frustum = build_view_frustum(frame_context.projection * frame_context.view);
    pipeline_render_state current_render_state{};
    apply_pipeline_render_state(current_render_state);

    {
        auto section = frame_profiler::measure_active("render_pipeline_flush_build_batches");
        if (!can_reuse_cached_batches()) {
            auto rebuild_section = frame_profiler::measure_active("render_pipeline_flush_batch_cache_miss");
            rebuild_cached_batches();
        }
        else {
            auto cache_hit_section = frame_profiler::measure_active("render_pipeline_flush_batch_cache_hit");
        }
    }

    {
        auto section = frame_profiler::measure_active("render_pipeline_flush_execute_batches");
        for (auto& batch : cached_batches_) {
            auto& renders = batch.renders;
            if (renders.empty())
                continue;

            std::vector<renderer*> visible_renders;
            visible_renders.reserve(renders.size());
            for (auto* render : renders) {
                if (!render || !render->get_node())
                    continue;
                if (intersects(frustum, render->get_node()->get_subtree_world_bounding_box()))
                    visible_renders.push_back(render);
            }

            if (visible_renders.empty())
                continue;

            const pipeline_render_state batch_render_state = make_pipeline_render_state(
                batch.key.blend_mode,
                batch.key.cull_mode,
                batch.key.depth_write_enabled);
            if (batch_render_state.blend_mode != current_render_state.blend_mode
                || batch_render_state.cull_mode != current_render_state.cull_mode
                || batch_render_state.depth_write_enabled != current_render_state.depth_write_enabled) {
                apply_pipeline_render_state(batch_render_state);
                current_render_state = batch_render_state;
            }

          if (visible_renders.size() == 1) {
                auto draw_section = frame_profiler::measure_active("render_pipeline_flush_draw_single");
             auto single_frame_context = frame_context;
                if (scene_context && visible_renders.front()->uses_gpu_driven_positions()) {
                    const GLuint physics_ssbo = scene_context->get_render_ssbo();
                    const size_t physics_index = scene_context->get_renderer_physics_index(visible_renders.front());
                    if (physics_ssbo != 0) {
                        single_frame_context.use_gpu_positions = true;
                        single_frame_context.physics_ssbo = physics_ssbo;
                      single_frame_context.physics_body_index = physics_index != static_cast<size_t>(-1)
                            ? static_cast<int>(physics_index)
                            : -1;
                    }
                }

              visible_renders.front()->draw(single_frame_context, pre_draw);
            }
            else {
                auto draw_section = frame_profiler::measure_active("render_pipeline_flush_draw_instanced");
                bool use_gpu_positions = false;
                int instance_base_index = -1;
                GLuint physics_ssbo = 0;

                if (scene_context) {
                    physics_ssbo = scene_context->get_render_ssbo();
                   const bool gpu_driven_batch = std::ranges::all_of(visible_renders, [](const renderer* render) {
                        return render && render->uses_gpu_driven_positions();
                    });
                    if (gpu_driven_batch && physics_ssbo != 0) {
                        use_gpu_positions = true;
                      const size_t first_index = scene_context->get_renderer_physics_index(visible_renders.front());
                        if (first_index != static_cast<size_t>(-1)) {
                            instance_base_index = static_cast<int>(first_index);

                           for (size_t i = 1; i < visible_renders.size(); ++i) {
                                const size_t expected = first_index + i;
                                if (scene_context->get_renderer_physics_index(visible_renders[i]) != expected) {
                                    instance_base_index = -1;
                                    break;
                                }
                            }
                        }
                    }
                }

                {
                    auto update_instances_section = frame_profiler::measure_active("render_pipeline_flush_update_instances");
                    const auto original_renders = batch.renders;
                    batch.renders = visible_renders;
                    update_cached_batch_instances(batch, use_gpu_positions);
                    batch.renders = original_renders;
                }

                {
                    auto submit_instanced_section = frame_profiler::measure_active("render_pipeline_flush_submit_instanced");
                    instance_manager_.draw_instanced(batch.key.shader_ptr, batch.key.mesh_ptr, visible_renders, batch.instance_models, batch.instance_physics_indices, frame_context, physics_ssbo, instance_base_index, use_gpu_positions, pre_draw);
                }
            }
        }
    }

    apply_pipeline_render_state(pipeline_render_state{});
}
