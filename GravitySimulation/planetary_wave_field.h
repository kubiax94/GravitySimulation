#pragma once

#include <glad/glad.h>
#include <glm/vec2.hpp>

class compute_shader;

struct planetary_wave_update_context
{
    GLuint support_atlas_texture = 0;
    GLuint forcing_texture = 0;
    GLuint water_domain_texture = 0;
    GLuint water_level_texture = 0;
    GLuint tidal_height_texture = 0;
    GLuint water_veto_texture = 0;
    GLuint region_id_texture = 0;
    GLuint shore_distance_texture = 0;
    float dt = 0.016f;
    float time_seconds = 0.0f;
    float propagation_speed = 1.15f;
    float forcing_scale = 0.42f;
    float open_water_damping = 0.92f;
    float shore_damping = 3.1f;
    float shore_transition_distance = 0.05f;
    float solver_forcing_scale = 0.72f;
};

class planetary_wave_field
{
    compute_shader* propagation_shader_ = nullptr;
    compute_shader* render_filter_shader_ = nullptr;
    GLuint wave_state_a_ = 0;
    GLuint wave_state_b_ = 0;
    GLuint render_wave_state_texture_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool source_is_a_ = true;

    void ensure_texture(GLuint& texture_id, int width, int height);
   void rebuild_render_wave_state_texture(GLuint water_domain_texture, GLuint region_id_texture);
    void release_textures();

public:
    planetary_wave_field() = default;
    planetary_wave_field(const planetary_wave_field&) = delete;
    planetary_wave_field& operator=(const planetary_wave_field&) = delete;
    planetary_wave_field(planetary_wave_field&& other) noexcept;
    planetary_wave_field& operator=(planetary_wave_field&& other) noexcept;

    void initialize(compute_shader* propagation_shader, compute_shader* render_filter_shader = nullptr);
    void resize_if_needed(int width, int height);
    void reset();
    void update(const planetary_wave_update_context& context);
    [[nodiscard]] GLuint get_wave_state_texture() const;
    [[nodiscard]] GLuint get_render_wave_state_texture() const;
    [[nodiscard]] bool is_valid() const { return propagation_shader_ != nullptr && width_ > 0 && height_ > 0; }
    [[nodiscard]] glm::ivec2 get_resolution() const { return glm::ivec2(width_, height_); }

    ~planetary_wave_field();
};
