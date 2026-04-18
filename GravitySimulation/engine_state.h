#pragma once

class engine;

class engine_state
{
public:
    virtual ~engine_state() = default;

    virtual void on_enter(engine& engine) = 0;
    virtual void on_exit(engine& engine) = 0;

    virtual void handle_input(engine& engine, float dt) = 0;
    virtual void fixed_update(engine& engine, float dt) = 0;
    virtual void update(engine& engine, float dt) = 0;
    virtual void render(engine& engine) = 0;
};