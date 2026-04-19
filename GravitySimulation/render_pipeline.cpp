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

void render_pipeline::begin_frame() {
    items_.clear();
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

            if (renders.size() == 1) {
                auto draw_section = frame_profiler::measure_active("render_pipeline_flush_draw_single");
             auto single_frame_context = frame_context;
                if (scene_context && renders.front()->uses_gpu_driven_positions()) {
                    const GLuint physics_ssbo = scene_context->get_render_ssbo();
                    const size_t physics_index = scene_context->get_renderer_physics_index(renders.front());
                    if (physics_ssbo != 0) {
                        single_frame_context.use_gpu_positions = true;
                        single_frame_context.physics_ssbo = physics_ssbo;
                      single_frame_context.physics_body_index = physics_index != static_cast<size_t>(-1)
                            ? static_cast<int>(physics_index)
                            : -1;
                    }
                }

                renders.front()->draw(single_frame_context, pre_draw);
            }
            else {
                auto draw_section = frame_profiler::measure_active("render_pipeline_flush_draw_instanced");
                bool use_gpu_positions = false;
                int instance_base_index = -1;
                GLuint physics_ssbo = 0;

                if (scene_context) {
                    physics_ssbo = scene_context->get_render_ssbo();
                  const bool gpu_driven_batch = std::ranges::all_of(renders, [](const renderer* render) {
                        return render && render->uses_gpu_driven_positions();
                    });
                    if (gpu_driven_batch && physics_ssbo != 0) {
                        use_gpu_positions = true;
                        const size_t first_index = scene_context->get_renderer_physics_index(renders.front());
                        if (first_index != static_cast<size_t>(-1)) {
                            instance_base_index = static_cast<int>(first_index);

                            for (size_t i = 1; i < renders.size(); ++i) {
                                const size_t expected = first_index + i;
                                if (scene_context->get_renderer_physics_index(renders[i]) != expected) {
                                    instance_base_index = -1;
                                    break;
                                }
                            }
                        }
                    }
                }

                {
                    auto update_instances_section = frame_profiler::measure_active("render_pipeline_flush_update_instances");
                    update_cached_batch_instances(batch, use_gpu_positions);
                }

                {
                    auto submit_instanced_section = frame_profiler::measure_active("render_pipeline_flush_submit_instanced");
                  instance_manager_.draw_instanced(batch.key.shader_ptr, batch.key.mesh_ptr, renders, batch.instance_models, batch.instance_physics_indices, frame_context, physics_ssbo, instance_base_index, use_gpu_positions, pre_draw);
                }
            }

    apply_pipeline_render_state(pipeline_render_state{});
        }
    }
}