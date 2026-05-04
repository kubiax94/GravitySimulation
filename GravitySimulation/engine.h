#pragma once

#include <memory>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "sim_time.h"
#include "engine_ui.h"
#include "frame_profiler.h"

class engine_state;

class engine
{
    GLFWwindow* window_ = nullptr;
    sim::time_sim time_;
    std::unique_ptr<engine_state> current_state_;
    std::unique_ptr<engine_state> pending_state_;
    frame_profiler frame_profiler_;
    engine_ui engine_ui_;
    bool vsync_enabled_ = true;
    bool processing_frame_ = false;

    void apply_state_change(std::unique_ptr<engine_state> next_state);

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_buttons_callback(GLFWwindow* window, int button, int action, int mods);
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

public:
    ~engine();

    bool init(int width, int height, const std::string& title);
    void run();
    void shutdown();

    void change_state(std::unique_ptr<engine_state> next_state);

    GLFWwindow* get_window() const { return window_; }
    sim::time_sim& get_time() { return time_; }
    const sim::time_sim& get_time() const { return time_; }
    frame_profiler& get_frame_profiler() { return frame_profiler_; }
    const frame_profiler& get_frame_profiler() const { return frame_profiler_; }
    engine_ui& get_ui() { return engine_ui_; }
    const engine_ui& get_ui() const { return engine_ui_; }
};
