#include "engine.h"

#include <glad/glad.h>

#include "engine_state.h"
#include "input_system.h"

void engine::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS)
        input_system::on_key_press(key);
    else if (action == GLFW_RELEASE)
        input_system::on_key_release(key);
}

void engine::mouse_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    input_system::on_mouse_move(xpos, ypos);
}

void engine::mouse_buttons_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS)
        input_system::on_mouse_button_press(button);
    else if (action == GLFW_RELEASE)
        input_system::on_mouse_button_release(button);
}

void engine::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

engine::~engine() {
    shutdown();
}

bool engine::init(int width, int height, const std::string& title) {
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        return false;
    }

    glfwSwapInterval(1);
    glfwFocusWindow(window_);

    glfwSetKeyCallback(window_, key_callback);
    glfwSetCursorPosCallback(window_, mouse_pos_callback);
    glfwSetMouseButtonCallback(window_, mouse_buttons_callback);
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    return true;
}

void engine::run() {
    if (!window_ || !current_state_)
        return;

    while (!glfwWindowShouldClose(window_)) {
        {
            auto timer = frame_profiler_.measure("frame_total");

            {
                auto section = frame_profiler_.measure("poll_events");
                glfwPollEvents();
            }

            time_.update_time(static_cast<float>(glfwGetTime()));

            {
                auto section = frame_profiler_.measure("handle_input");
                current_state_->handle_input(*this, time_.delta_time);
            }

            {
                auto section = frame_profiler_.measure("fixed_update_total");
                while (time_.should_fixed_update()) {
                    auto fixed_section = frame_profiler_.measure("fixed_update_step");
                    current_state_->fixed_update(*this, time_.fixed_delta_time);
                    time_.reduce_accumulator();
                }
            }

            {
                auto section = frame_profiler_.measure("update");
                current_state_->update(*this, time_.delta_time);
            }

            {
                auto section = frame_profiler_.measure("render");
                current_state_->render(*this);
            }

            {
                auto section = frame_profiler_.measure("swap_buffers");
                glfwSwapBuffers(window_);
            }
        }

        frame_profiler_.end_frame();
    }
}

void engine::shutdown() {
    if (current_state_) {
        current_state_->on_exit(*this);
        current_state_.reset();
    }

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void engine::change_state(std::unique_ptr<engine_state> next_state) {
    if (current_state_) {
        current_state_->on_exit(*this);
        current_state_.reset();
    }

    current_state_ = std::move(next_state);
    if (current_state_)
        current_state_->on_enter(*this);
}