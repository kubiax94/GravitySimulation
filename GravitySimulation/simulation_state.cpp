#include "simulation_state.h"

#include <glad/glad.h>

#include "engine.h"
#include "g_shape.h"
#include "galactic_simulation_test.h"

void simulation_state::on_enter(engine& engine) {
    scene_ = std::make_unique<scene>(&engine.get_time());

    cam_node_ = scene_->create_scene_node("cam");
    grid_node_ = scene_->create_scene_node("grid");

    cam_node_->add_component<Camera>(cam_node_);
    cam_node_->set_global_position(glm::vec3(0.f, 280.f, 400.f));
    cam_node_->set_global_rotation(glm::vec3(-45.f, 0.f, 0.f));
    cam_ = cam_node_->find_component<Camera>();

    auto grid_data = g_shape::generate_grid_lines(64, 50.f);
    grid_mesh_ = std::make_unique<Mesh>(grid_data);
    grid_mesh_->type = MeshType::LINES;
    grid_shader_ = std::make_unique<shader>("default.vs.shader", "default.fs.shader");
    grid_node_->add_component<renderer>(grid_node_, grid_shader_.get(), grid_mesh_.get());
    grid_node_->set_global_position(glm::vec3(0.f, .001f, 0.f));

    std::vector<renderer*> unused_planet_renders;
    simtest::init_gravity_test(scene_.get(), unused_planet_renders);
    const int stress_count = 1000;
    simtest::stress_test(scene_.get(), unused_planet_renders, stress_count);

    sun_node_ = scene_->find_scene_node("Sun");
}

void simulation_state::on_exit(engine& engine) {
    cam_ = nullptr;
    cam_node_ = nullptr;
    grid_node_ = nullptr;
    sun_node_ = nullptr;
    grid_mesh_.reset();
    grid_shader_.reset();
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
        render_pipeline_.flush(cam_, [&](shader& s) {
            s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
            s.set_uni_vec3("viewPos", cam_->get_transform()->get_global_position());
            if (sun_node_)
                s.set_uni_vec3("lightPos", sun_node_->get_global_position());
            s.set_uni_float("intensity", 0.75f);
        });
    }
}