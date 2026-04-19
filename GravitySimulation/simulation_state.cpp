#include "simulation_state.h"

#include <glad/glad.h>

#include "engine.h"

simulation_state::simulation_state(std::unique_ptr<scene> scene)
    : scene_(std::move(scene)) {
}

void simulation_state::on_enter(engine& engine) {
    if (!scene_)
        scene_ = std::make_unique<scene>(&engine.get_time());

    scene_->init();
    cam_ = scene_->get_main_camera();
}

void simulation_state::on_exit(engine& engine) {
    cam_ = nullptr;
    scene_.reset();
}

void simulation_state::handle_input(engine& engine, float dt) {
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
        render_pipeline_.flush(cam_, scene_.get(), [&](shader& s) {
            s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
            s.set_uni_vec3("viewPos", cam_->get_transform()->get_global_position());
            s.set_uni_vec3("lightPos", cam_->get_transform()->get_global_position() + glm::vec3(0.f, 100.f, 100.f));
            s.set_uni_float("intensity", 0.75f);
        });
    }
}
