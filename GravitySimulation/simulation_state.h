#pragma once

#include <memory>

#include "engine_state.h"
#include "render_pipeline.h"
#include "Scene.h"
#include "Shader.h"
#include "Mesh.h"

class simulation_state : public engine_state
{
    std::unique_ptr<scene> scene_;
    Camera* cam_ = nullptr;

    render_pipeline render_pipeline_;
    bool previous_left_mouse_down_ = false;
    bool previous_escape_down_ = false;
    bool focus_active_ = false;
    float focus_elapsed_ = 0.f;
    float focus_duration_ = 0.85f;
    glm::vec3 focus_start_position_ = glm::vec3(0.f);
    glm::vec3 focus_target_position_ = glm::vec3(0.f);
    glm::vec3 focus_target_offset_ = glm::vec3(0.f);
    glm::vec3 focus_look_at_ = glm::vec3(0.f);
    scene_node* focus_target_node_ = nullptr;
    scene_node* attached_camera_parent_ = nullptr;

    void try_begin_focus();
    void update_camera_focus(float dt);
    void cancel_camera_focus();
    void detach_camera_parent();

public:
    simulation_state() = default;
    explicit simulation_state(std::unique_ptr<scene> scene);

    void on_enter(engine& engine) override;
    void on_exit(engine& engine) override;
    void handle_input(engine& engine, float dt) override;
    void fixed_update(engine& engine, float dt) override;
    void update(engine& engine, float dt) override;
    void render(engine& engine) override;
};