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
    scene_node* cam_node_ = nullptr;
    scene_node* grid_node_ = nullptr;
    scene_node* sun_node_ = nullptr;
    Camera* cam_ = nullptr;

    std::unique_ptr<shader> grid_shader_;
    std::unique_ptr<Mesh> grid_mesh_;

    render_pipeline render_pipeline_;

public:
    void on_enter(engine& engine) override;
    void on_exit(engine& engine) override;
    void handle_input(engine& engine, float dt) override;
    void fixed_update(engine& engine, float dt) override;
    void update(engine& engine, float dt) override;
    void render(engine& engine) override;
};