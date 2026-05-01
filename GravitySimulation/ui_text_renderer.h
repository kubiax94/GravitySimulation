#pragma once

#include <string>

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

class engine;
class scene;
class shader;
class font_resource;

class ui_text_renderer
{
public:
    struct text_bounds {
        glm::vec2 size = glm::vec2(0.0f);
        float max_line_width = 0.0f;
        float line_height = 0.0f;
        int line_count = 0;
    };

private:
    shader* text_shader_ = nullptr;
    font_resource* default_font_ = nullptr;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    bool initialized_ = false;

    static std::string resolve_default_font_path();
    bool ensure_gpu_objects();

public:
    ~ui_text_renderer();

    bool initialize(scene& scene_context);
    void shutdown();
    bool is_ready() const;
    font_resource* get_default_font() const { return default_font_; }
    text_bounds measure_text(const std::string& text, float scale = 1.0f) const;
    void render_text(engine& engine, const std::string& text, const glm::vec2& top_left, float scale = 1.0f, const glm::vec3& color = glm::vec3(1.0f));
};
