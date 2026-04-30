#include "planetary_wave_field.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "compute_shader.h"

planetary_wave_field::planetary_wave_field(planetary_wave_field&& other) noexcept
    : propagation_shader_(other.propagation_shader_),
      render_filter_shader_(other.render_filter_shader_),
      wave_state_a_(other.wave_state_a_),
      wave_state_b_(other.wave_state_b_),
      render_wave_state_texture_(other.render_wave_state_texture_),
      width_(other.width_),
      height_(other.height_),
      source_is_a_(other.source_is_a_)
{
    other.propagation_shader_ = nullptr;
    other.render_filter_shader_ = nullptr;
    other.wave_state_a_ = 0;
    other.wave_state_b_ = 0;
    other.render_wave_state_texture_ = 0;
    other.width_ = 0;
    other.height_ = 0;
    other.source_is_a_ = true;
}

planetary_wave_field& planetary_wave_field::operator=(planetary_wave_field&& other) noexcept
{
    if (this != &other) {
        release_textures();
        propagation_shader_ = other.propagation_shader_;
        render_filter_shader_ = other.render_filter_shader_;
        wave_state_a_ = other.wave_state_a_;
        wave_state_b_ = other.wave_state_b_;
        render_wave_state_texture_ = other.render_wave_state_texture_;
        width_ = other.width_;
        height_ = other.height_;
        source_is_a_ = other.source_is_a_;
        other.propagation_shader_ = nullptr;
        other.render_filter_shader_ = nullptr;
        other.wave_state_a_ = 0;
        other.wave_state_b_ = 0;
        other.render_wave_state_texture_ = 0;
        other.width_ = 0;
        other.height_ = 0;
        other.source_is_a_ = true;
    }

    return *this;
}

void planetary_wave_field::ensure_texture(GLuint& texture_id, int width, int height)
{
    if (texture_id == 0)
        glGenTextures(1, &texture_id);

    if (texture_id == 0)
        return;

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, nullptr);
    const float clear_value[2] = { 0.0f, 0.0f };
    glClearTexImage(texture_id, 0, GL_RG, GL_FLOAT, clear_value);
}

void planetary_wave_field::release_textures()
{
    if (wave_state_a_ != 0) {
        glDeleteTextures(1, &wave_state_a_);
        wave_state_a_ = 0;
    }
    if (wave_state_b_ != 0) {
        glDeleteTextures(1, &wave_state_b_);
        wave_state_b_ = 0;
    }
    if (render_wave_state_texture_ != 0) {
        glDeleteTextures(1, &render_wave_state_texture_);
        render_wave_state_texture_ = 0;
    }
    width_ = 0;
    height_ = 0;
    source_is_a_ = true;
}

void planetary_wave_field::rebuild_render_wave_state_texture(GLuint water_domain_texture, GLuint region_id_texture)
{
    if (width_ <= 0 || height_ <= 0)
        return;

    ensure_texture(render_wave_state_texture_, width_, height_);
    if (render_wave_state_texture_ == 0)
        return;

    const GLuint source_texture = get_wave_state_texture();
    if (source_texture == 0)
        return;

    if (water_domain_texture == 0 || region_id_texture == 0 || !render_filter_shader_ || !render_filter_shader_->is_vaild()) {
        glCopyImageSubData(source_texture, GL_TEXTURE_2D, 0, 0, 0, 0,
            render_wave_state_texture_, GL_TEXTURE_2D, 0, 0, 0, 0,
            width_, height_, 1);
        return;
    }

    render_filter_shader_->use();
    render_filter_shader_->set_uni_vec2("waveResolution", glm::vec2(static_cast<float>(width_), static_cast<float>(height_)));
    render_filter_shader_->set_uni_int("sourceWaveStateTexture", 0);
    render_filter_shader_->set_uni_int("waterDomainTexture", 1);
    render_filter_shader_->set_uni_int("regionIdTexture", 2);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, water_domain_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, region_id_texture);
    glActiveTexture(GL_TEXTURE0);

    glBindImageTexture(0, render_wave_state_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
    const GLuint groups_x = static_cast<GLuint>((width_ + 15) / 16);
    const GLuint groups_y = static_cast<GLuint>((height_ + 15) / 16);
    render_filter_shader_->dispatch({ groups_x, groups_y, 1u });
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void planetary_wave_field::initialize(compute_shader* propagation_shader, compute_shader* render_filter_shader)
{
    propagation_shader_ = propagation_shader;
    render_filter_shader_ = render_filter_shader;
}

void planetary_wave_field::resize_if_needed(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width_ == width && height_ == height && wave_state_a_ != 0 && wave_state_b_ != 0)
        return;

    release_textures();
    ensure_texture(wave_state_a_, width, height);
    ensure_texture(wave_state_b_, width, height);
    width_ = width;
    height_ = height;
    const float clear_value[2] = { 0.0f, 0.0f };
    if (render_wave_state_texture_ != 0)
        glClearTexImage(render_wave_state_texture_, 0, GL_RG, GL_FLOAT, clear_value);
}

void planetary_wave_field::reset()
{
    if (wave_state_a_ == 0 || wave_state_b_ == 0)
        return;

    const float clear_value[2] = { 0.0f, 0.0f };
    glClearTexImage(wave_state_a_, 0, GL_RG, GL_FLOAT, clear_value);
    glClearTexImage(wave_state_b_, 0, GL_RG, GL_FLOAT, clear_value);
    if (render_wave_state_texture_ != 0)
        glClearTexImage(render_wave_state_texture_, 0, GL_RG, GL_FLOAT, clear_value);
    source_is_a_ = true;
}

void planetary_wave_field::update(const planetary_wave_update_context& context)
{
    if (!propagation_shader_ || !propagation_shader_->is_vaild())
        return;
    if (context.water_domain_texture == 0 || context.region_id_texture == 0 || context.shore_distance_texture == 0)
        return;
    if (wave_state_a_ == 0 || wave_state_b_ == 0)
        return;

    const GLuint source_texture = source_is_a_ ? wave_state_a_ : wave_state_b_;
    const GLuint target_texture = source_is_a_ ? wave_state_b_ : wave_state_a_;

    propagation_shader_->use();
    propagation_shader_->set_uni_float("dt", context.dt);
    propagation_shader_->set_uni_float("timeSeconds", context.time_seconds);
    propagation_shader_->set_uni_float("propagationSpeed", context.propagation_speed);
    propagation_shader_->set_uni_float("forcingScale", context.forcing_scale);
    propagation_shader_->set_uni_float("openWaterDamping", context.open_water_damping);
    propagation_shader_->set_uni_float("shoreDamping", context.shore_damping);
    propagation_shader_->set_uni_float("shoreTransitionDistance", context.shore_transition_distance);
    propagation_shader_->set_uni_vec2("waveResolution", glm::vec2(static_cast<float>(width_), static_cast<float>(height_)));
    propagation_shader_->set_uni_int("waveStateTexture", 0);
    propagation_shader_->set_uni_int("supportAtlasTexture", 1);
    propagation_shader_->set_uni_int("forcingTexture", 2);
    propagation_shader_->set_uni_int("waterDomainTexture", 3);
    propagation_shader_->set_uni_int("waterLevelTexture", 4);
    propagation_shader_->set_uni_int("tidalHeightTexture", 5);
    propagation_shader_->set_uni_int("waterVetoTexture", 6);
    propagation_shader_->set_uni_int("regionIdTexture", 7);
    propagation_shader_->set_uni_int("shoreDistanceTexture", 8);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, context.support_atlas_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, context.forcing_texture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, context.water_domain_texture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, context.water_level_texture);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, context.tidal_height_texture);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, context.water_veto_texture);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, context.region_id_texture);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, context.shore_distance_texture);
    glActiveTexture(GL_TEXTURE0);

    glBindImageTexture(0, target_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
    const GLuint groups_x = static_cast<GLuint>((width_ + 15) / 16);
    const GLuint groups_y = static_cast<GLuint>((height_ + 15) / 16);
    propagation_shader_->dispatch({ groups_x, groups_y, 1u });
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);

    for (GLuint texture_unit = 0; texture_unit <= 8; ++texture_unit) {
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);

    source_is_a_ = !source_is_a_;
    rebuild_render_wave_state_texture(context.water_domain_texture, context.region_id_texture);
}

GLuint planetary_wave_field::get_wave_state_texture() const
{
    return source_is_a_ ? wave_state_a_ : wave_state_b_;
}

GLuint planetary_wave_field::get_render_wave_state_texture() const
{
    return render_wave_state_texture_ != 0 ? render_wave_state_texture_ : get_wave_state_texture();
}

planetary_wave_field::~planetary_wave_field()
{
    release_textures();
}
