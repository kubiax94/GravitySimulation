#include "simulation_state.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/quaternion.hpp>

#include "cloth_scene.h"
#include "engine.h"
#include "fluid_scene.h"
#include "galactic_scene.h"
#include "galactic_stress_scene.h"
#include "gpu_fluid_system_component.h"
#include "gpu_particle_system_component.h"
#include "input_system.h"

namespace {
struct focus_pick_result {
    renderer* render = nullptr;
    glm::vec3 world_position = glm::vec3(0.f);
    float world_radius = 1.f;
};

float ease_out_cubic(float t) {
    const float inv = 1.f - glm::clamp(t, 0.f, 1.f);
    return 1.f - inv * inv * inv;
}

std::unique_ptr<scene> create_example_scene(simulation_state::example_scene_kind scene_kind, sim::time* time) {
    switch (scene_kind) {
    case simulation_state::example_scene_kind::fluid:
        return std::make_unique<fluid_scene>(time);
    case simulation_state::example_scene_kind::cloth:
        return std::make_unique<cloth_scene>(time);
    case simulation_state::example_scene_kind::galactic:
        return std::make_unique<galactic_scene>(time);
    case simulation_state::example_scene_kind::galactic_stress:
        return std::make_unique<galactic_stress_scene>(time);
    }

    return std::make_unique<fluid_scene>(time);
}

const char* get_scene_name(simulation_state::example_scene_kind scene_kind) {
    switch (scene_kind) {
    case simulation_state::example_scene_kind::fluid:
        return "Fluid";
    case simulation_state::example_scene_kind::cloth:
        return "Cloth";
    case simulation_state::example_scene_kind::galactic:
        return "Galactic";
    case simulation_state::example_scene_kind::galactic_stress:
        return "Galactic Stress";
    }

    return "Fluid";
}

std::string build_window_title(simulation_state::example_scene_kind scene_kind) {
    return std::string("GravitySimulation - ") + get_scene_name(scene_kind)
        + " [F1 Fluid | F2 Cloth | F3 Galactic | F4 Stress]";
}

bool poll_scene_switch_key(int glfw_key, bool& previous_down) {
    const bool is_down = input_system::is_key_down(glfw_key);
    const bool pressed = is_down && !previous_down;
    previous_down = is_down;
    return pressed;
}

float estimate_renderer_radius(const renderer& render) {
    const glm::mat4 model = render.get_visual_model_matrix_without_translation();
    return std::max({
        glm::length(glm::vec3(model[0])),
        glm::length(glm::vec3(model[1])),
        glm::length(glm::vec3(model[2])),
        1.0f
    });
}

void orient_camera_towards(Camera& camera, const glm::vec3& target_position) {
    const glm::vec3 camera_position = camera.get_node()->get_global_position();
    const glm::vec3 delta = target_position - camera_position;
    if (glm::dot(delta, delta) <= 0.0001f)
        return;

    const glm::vec3 direction = glm::normalize(delta);
    const float yaw = glm::degrees(std::atan2(direction.z, direction.x));
    const float pitch = glm::degrees(std::asin(glm::clamp(direction.y, -1.f, 1.f)));
    const glm::quat q_pitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1.f, 0.f, 0.f));
    const glm::quat q_yaw = glm::angleAxis(glm::radians(yaw), glm::vec3(0.f, 1.f, 0.f));
    const glm::vec3 euler = glm::degrees(glm::eulerAngles(q_yaw * q_pitch));

    camera.Yaw = yaw;
    camera.Pitch = pitch;
    camera.get_node()->set_global_rotation(euler);
}

std::optional<focus_pick_result> pick_renderer_from_mouse(const scene& scene_context, Camera& camera) {
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return std::nullopt;

    int window_width = 0;
    int window_height = 0;
    glfwGetWindowSize(window, &window_width, &window_height);
    if (window_width <= 0 || window_height <= 0)
        return std::nullopt;

    const glm::vec3 mouse_pos = input_system::get_mouse_pos();
    const float aspect = static_cast<float>(window_width) / static_cast<float>(window_height);
    const glm::mat4 view = camera.GetViewMatrix();
    const glm::mat4 projection = camera.GetProjectionMatrix(aspect);

    float best_score = std::numeric_limits<float>::max();
    std::optional<focus_pick_result> best_pick;

    for (auto* render : scene_context.get_renderers()) {
        if (!render || !render->get_node() || !render->get_mesh() || render->get_mesh()->type == MeshType::LINES)
            continue;

        const glm::vec3 world_position = render->get_node()->get_global_position();
        const glm::vec4 clip = projection * view * glm::vec4(world_position, 1.f);
        if (clip.w <= 0.0001f)
            continue;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.f || ndc.z > 1.f)
            continue;

        const glm::vec2 screen(
            (ndc.x * 0.5f + 0.5f) * static_cast<float>(window_width),
            (1.f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(window_height));
        const glm::vec2 delta = screen - glm::vec2(mouse_pos.x, mouse_pos.y);
        const float distance_sq = glm::dot(delta, delta);

        const float world_radius = estimate_renderer_radius(*render);
        const float projected_radius = std::max(10.f, world_radius * 160.f / std::max(clip.w, 1.f));
        if (distance_sq > projected_radius * projected_radius)
            continue;

        const float score = distance_sq + (ndc.z + 1.f) * 250.f;
        if (score < best_score) {
            best_score = score;
            best_pick = focus_pick_result{ render, world_position, world_radius };
        }
    }

    return best_pick;
}
}

simulation_state::simulation_state(std::unique_ptr<scene> scene)
    : scene_(std::move(scene)) {
}

simulation_state::simulation_state(example_scene_kind scene_kind)
    : scene_kind_(scene_kind) {
}

void simulation_state::on_enter(engine& engine) {
    if (!scene_)
        scene_ = create_example_scene(scene_kind_, &engine.get_time());

    scene_->init();
    cam_ = scene_->get_main_camera();

    if (GLFWwindow* window = engine.get_window())
        glfwSetWindowTitle(window, build_window_title(scene_kind_).c_str());
}

void simulation_state::on_exit(engine& engine) {
    cam_ = nullptr;
    scene_.reset();
}

void simulation_state::handle_input(engine& engine, float dt) {
    if (!cam_ || !scene_)
        return;

    constexpr std::array<int, 4> scene_switch_keys = {
        GLFW_KEY_F1,
        GLFW_KEY_F2,
        GLFW_KEY_F3,
        GLFW_KEY_F4
    };

    for (size_t i = 0; i < scene_switch_keys.size(); ++i) {
        if (poll_scene_switch_key(scene_switch_keys[i], previous_scene_switch_down_[i])) {
            switch_scene(engine, static_cast<example_scene_kind>(i));
            return;
        }
    }

    const bool left_mouse_down = input_system::is_button_down(GLFW_MOUSE_BUTTON_LEFT);
    const bool left_mouse_pressed = left_mouse_down && !previous_left_mouse_down_;
    previous_left_mouse_down_ = left_mouse_down;
    const bool escape_down = input_system::is_key_down(GLFW_KEY_ESCAPE);
    const bool escape_pressed = escape_down && !previous_escape_down_;
    previous_escape_down_ = escape_down;

    if (escape_pressed)
        detach_camera_parent();

    if (left_mouse_pressed && !input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT))
        try_begin_focus();

    const bool manual_camera_input = input_system::is_button_down(GLFW_MOUSE_BUTTON_RIGHT)
        || input_system::is_key_down(GLFW_KEY_W)
        || input_system::is_key_down(GLFW_KEY_A)
        || input_system::is_key_down(GLFW_KEY_S)
        || input_system::is_key_down(GLFW_KEY_D);
    if (manual_camera_input)
        cancel_camera_focus();

    if (cam_)
        cam_->process_input(dt);
}

void simulation_state::fixed_update(engine& engine, float dt) {
    if (scene_)
        scene_->update();
}

void simulation_state::update(engine& engine, float dt) {
    if (!scene_)
        return;

    auto section = engine.get_frame_profiler().measure("scene_sync_render");
    scene_->sync_render();
    update_camera_focus(dt);
}

void simulation_state::render(engine& engine) {
    if (!cam_ || !scene_)
        return;

    auto& profiler = engine.get_frame_profiler();

    {
        auto section = profiler.measure("render_clear");
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    {
        auto section = profiler.measure("render_pipeline_begin_frame");
        render_pipeline_.begin_frame();
    }

    {
        auto section = profiler.measure("render_pipeline_submit");
        for (auto* render : scene_->get_renderers())
            render_pipeline_.submit(render);
    }

    {
        auto section = profiler.measure("render_pipeline_flush");
        const glm::vec3 light_position = scene_->has_primary_light()
            ? scene_->get_primary_light_position()
            : cam_->get_transform()->get_global_position() + glm::vec3(0.f, 100.f, 100.f);
        const glm::vec3 light_color = scene_->has_primary_light()
            ? scene_->get_primary_light_color()
            : glm::vec3(1.0f, .8f, .3f);
        const float light_intensity = scene_->has_primary_light()
            ? scene_->get_primary_light_intensity()
            : 0.75f;

        render_pipeline_.flush(cam_, scene_.get(), [&](shader& s) {
            s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
            s.set_uni_vec3("lightColor", light_color);
            s.set_uni_vec3("viewPos", cam_->get_transform()->get_global_position());
            s.set_uni_vec3("lightPos", light_position);
            s.set_uni_float("intensity", light_intensity);
        });
    }

    for (auto* system : scene_->get_gpu_particle_systems()) {
        if (system)
            system->draw(cam_);
    }

    for (auto* system : scene_->get_gpu_fluid_systems()) {
        if (system)
            system->draw(cam_);
    }
}

void simulation_state::try_begin_focus() {
    if (!scene_ || !cam_)
        return;

    const auto pick = pick_renderer_from_mouse(*scene_, *cam_);
    if (!pick)
        return;

    const glm::vec3 current_camera_position = cam_->get_node()->get_global_position();
    glm::vec3 direction_from_target = current_camera_position - pick->world_position;
    if (glm::dot(direction_from_target, direction_from_target) <= 0.0001f)
        direction_from_target = -cam_->get_node()->forward();

    direction_from_target = glm::normalize(direction_from_target);
    const float focus_distance = std::max(18.f, pick->world_radius * 10.f);
    const glm::vec3 focus_target_world_position = pick->world_position
        + direction_from_target * focus_distance
        + glm::vec3(0.f, pick->world_radius * 1.25f, 0.f);

    focus_target_node_ = pick->render ? pick->render->get_node() : nullptr;
    if (!focus_target_node_)
        return;

    cam_->get_node()->set_parent(focus_target_node_, true);
    attached_camera_parent_ = focus_target_node_;

    const glm::mat4 parent_inverse = glm::inverse(focus_target_node_->get_global_matrix_model());
    focus_start_position_ = cam_->get_node()->get_position();
    focus_target_position_ = glm::vec3(parent_inverse * glm::vec4(focus_target_world_position, 1.f));
    focus_target_offset_ = focus_target_position_;
    focus_look_at_ = pick->world_position;
    focus_elapsed_ = 0.f;
    focus_active_ = true;
}

void simulation_state::update_camera_focus(float dt) {
    if (!focus_active_ || !cam_)
        return;

    focus_elapsed_ += dt;
    if (focus_target_node_) {
        focus_look_at_ = focus_target_node_->get_global_position();
    }

    const float t = glm::clamp(focus_elapsed_ / focus_duration_, 0.f, 1.f);
    const float eased_t = ease_out_cubic(t);
    const glm::vec3 next_position = glm::mix(focus_start_position_, focus_target_position_, eased_t);

    cam_->get_node()->set_position(next_position);
    orient_camera_towards(*cam_, focus_look_at_);

   if (t >= 1.f) {
        focus_target_node_ = nullptr;
        focus_active_ = false;
   }
}

void simulation_state::cancel_camera_focus() {
    focus_active_ = false;
    focus_elapsed_ = 0.f;
   focus_target_node_ = nullptr;
}

void simulation_state::detach_camera_parent() {
    if (!cam_ || !scene_)
        return;

    cancel_camera_focus();
    if (attached_camera_parent_ || cam_->get_node()->get_parent() != scene_->get_root_node())
        cam_->get_node()->set_parent(scene_->get_root_node(), true);

    attached_camera_parent_ = nullptr;
}

void simulation_state::switch_scene(engine& engine, example_scene_kind next_scene_kind) {
    if (next_scene_kind == scene_kind_)
        return;

    engine.change_state(std::make_unique<simulation_state>(next_scene_kind));
}
