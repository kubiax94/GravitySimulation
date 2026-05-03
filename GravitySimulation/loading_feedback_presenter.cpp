#include "loading_feedback_presenter.h"

#include <iomanip>
#include <sstream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "engine.h"
#include "Scene.h"
#include "scene_loader.h"

namespace {
    void set_window_title(engine& engine, const std::string& title) {
        if (GLFWwindow* window = engine.get_window())
            glfwSetWindowTitle(window, title.c_str());
    }

    std::string build_loading_title(const std::string& base_title, const scene_loader& loader) {
        std::ostringstream stream;
        stream << base_title << " [Loading "
            << loader.get_completed_resources() << "/" << loader.get_total_resources()
            << " | " << std::fixed << std::setprecision(0) << loader.get_progress() * 100.0f << "%]";
        return stream.str();
    }
}

void loading_feedback_presenter::on_loading_begin(engine& engine, const scene& active_scene, const scene_loader& loader) {
}

void loading_feedback_presenter::on_loading_complete(engine& engine, const scene& active_scene, const scene_loader& loader) {
}

void loading_feedback_presenter::on_loading_failed(engine& engine, const scene& active_scene, const scene_loader& loader) {
}

void loading_feedback_presenter::render(engine& engine, const scene& active_scene, const scene_loader& loader) {
}

namespace {
    ui_loading_feedback_state build_runtime_loading_feedback_state(const scene_loader& loader, bool active, bool failed) {
        std::ostringstream progress_stream;
        progress_stream << std::fixed << std::setprecision(0) << loader.get_progress() * 100.0f;

        ui_loading_feedback_state state;
        state.active = active;
        state.failed = failed;
        state.title = "Runtime loading";
        state.status = failed ? "Loading failed" : "Loading resources asynchronously...";
        state.progress_text = progress_stream.str() + "%";
        state.progress = static_cast<float>(loader.get_progress());
        state.completed_resources = loader.get_completed_resources();
        state.total_resources = loader.get_total_resources();
        return state;
    }
}

void runtime_ui_loading_feedback::on_loading_begin(engine& engine, const scene& active_scene, const scene_loader& loader) {
    engine.get_ui().submit_loading_feedback(build_runtime_loading_feedback_state(loader, true, false));
}

void runtime_ui_loading_feedback::on_loading_update(engine& engine, const scene& active_scene, const scene_loader& loader) {
    engine.get_ui().submit_loading_feedback(build_runtime_loading_feedback_state(loader, true, false));
}

void runtime_ui_loading_feedback::on_loading_complete(engine& engine, const scene& active_scene, const scene_loader& loader) {
    engine.get_ui().clear_loading_feedback();
}

void runtime_ui_loading_feedback::on_loading_failed(engine& engine, const scene& active_scene, const scene_loader& loader) {
    engine.get_ui().submit_loading_feedback(build_runtime_loading_feedback_state(loader, true, true));
}

window_title_loading_feedback::window_title_loading_feedback(std::string base_title)
    : base_title_(std::move(base_title)) {
}

void window_title_loading_feedback::set_base_title(std::string base_title) {
    base_title_ = std::move(base_title);
}

void window_title_loading_feedback::on_loading_begin(engine& engine, const scene& active_scene, const scene_loader& loader) {
    active_ = true;
    if (base_title_.empty())
        base_title_ = "GravitySimulation";
    set_window_title(engine, build_loading_title(base_title_, loader));
}

void window_title_loading_feedback::on_loading_update(engine& engine, const scene& active_scene, const scene_loader& loader) {
    if (!active_)
        return;
    set_window_title(engine, build_loading_title(base_title_, loader));
}

void window_title_loading_feedback::on_loading_complete(engine& engine, const scene& active_scene, const scene_loader& loader) {
    active_ = false;
    if (!base_title_.empty())
        set_window_title(engine, base_title_);
}

void window_title_loading_feedback::on_loading_failed(engine& engine, const scene& active_scene, const scene_loader& loader) {
    active_ = false;
    if (!base_title_.empty())
        set_window_title(engine, base_title_ + " [Loading failed]");
}
