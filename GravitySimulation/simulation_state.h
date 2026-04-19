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