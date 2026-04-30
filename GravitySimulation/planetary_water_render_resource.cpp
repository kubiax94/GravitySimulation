#include "planetary_water_render_resource.h"

namespace {
    bool ensure_color_texture(texture& target, int width, int height) {
        return target.allocate_2d(
            width,
            height,
            GL_RGBA16F,
            GL_RGBA,
            GL_FLOAT,
            nullptr,
            GL_REPEAT,
            GL_CLAMP_TO_EDGE,
            GL_LINEAR,
            GL_LINEAR,
            false);
    }
}

planetary_water_render_resource::planetary_water_render_resource(const std::string& name)
    : asset(asset_type::TEXTURE, name) {
}

bool planetary_water_render_resource::load() {
    status_ = asset_status::LOADED;
    return true;
}

bool planetary_water_render_resource::finalize() {
    status_ = asset_status::LOADED;
    return true;
}

void planetary_water_render_resource::unload() {
    release_atlas_targets();
    asset::unload();
}

void planetary_water_render_resource::cleanup() {
    release_atlas_targets();
}

bool planetary_water_render_resource::is_vaild() {
    return atlas_targets_.framebuffer != 0 || status_ == asset_status::LOADED;
}

bool planetary_water_render_resource::ensure_atlas_targets(int width, int height) {
    if (width <= 0 || height <= 0)
        return false;

    if (atlas_targets_.framebuffer == 0)
        glGenFramebuffers(1, &atlas_targets_.framebuffer);

    if (atlas_targets_.width == width && atlas_targets_.height == height)
        return atlas_targets_.framebuffer != 0;

    if (!ensure_color_texture(atlas_targets_.atlas_texture, width, height)
        || !ensure_color_texture(atlas_targets_.atlas_ping_texture, width, height)
        || !ensure_color_texture(atlas_targets_.atlas_history_texture, width, height)
        || !ensure_color_texture(atlas_targets_.wave_forcing_texture, width, height)
        || !ensure_color_texture(atlas_targets_.wave_forcing_ping_texture, width, height)
        || !ensure_color_texture(atlas_targets_.wave_forcing_history_texture, width, height)
        || !atlas_targets_.tide_height_texture.allocate_2d(
            width,
            height,
            GL_R32F,
            GL_RED,
            GL_FLOAT,
            nullptr,
            GL_REPEAT,
            GL_CLAMP_TO_EDGE,
            GL_LINEAR,
            GL_LINEAR,
            false)) {
        return false;
    }

    const float clear_history[4] = { 0.f, 0.f, 0.f, 0.f };
    atlas_targets_.atlas_history_texture.clear(GL_RGBA, GL_FLOAT, clear_history);
    atlas_targets_.wave_forcing_history_texture.clear(GL_RGBA, GL_FLOAT, clear_history);
    const float clear_tide = 0.f;
    atlas_targets_.tide_height_texture.clear(GL_RED, GL_FLOAT, &clear_tide);

    glBindFramebuffer(GL_FRAMEBUFFER, atlas_targets_.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atlas_targets_.atlas_texture.get_id(), 0);
    const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, draw_buffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    atlas_targets_.width = width;
    atlas_targets_.height = height;
    status_ = asset_status::LOADED;
    return true;
}

void planetary_water_render_resource::release_atlas_targets() {
    if (atlas_targets_.framebuffer != 0) {
        glDeleteFramebuffers(1, &atlas_targets_.framebuffer);
        atlas_targets_.framebuffer = 0;
    }

    atlas_targets_.atlas_texture.cleanup();
    atlas_targets_.atlas_ping_texture.cleanup();
    atlas_targets_.atlas_history_texture.cleanup();
    atlas_targets_.wave_forcing_texture.cleanup();
    atlas_targets_.wave_forcing_ping_texture.cleanup();
    atlas_targets_.wave_forcing_history_texture.cleanup();
    atlas_targets_.tide_height_texture.cleanup();
    atlas_targets_.width = 0;
    atlas_targets_.height = 0;
}
