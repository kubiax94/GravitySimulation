#pragma once

#include <glad/glad.h>

#include <string>
#include <vector>

#include "asset.h"

class texture : public asset
{
    GLuint id_ = 0;
    int width_ = 0;
    int height_ = 0;
    int channels_ = 4;
    GLenum target_ = GL_TEXTURE_2D;
    GLenum internal_format_ = GL_RGBA8;
    GLenum data_format_ = GL_RGBA;
    GLenum data_type_ = GL_UNSIGNED_BYTE;
    std::vector<unsigned char> pixel_data_;

public:
    explicit texture(const std::string& texture_path = "", const std::string& name = "");
    ~texture() override = default;

    bool load() override;
    bool finalize() override;
    void unload() override;
    void cleanup() override;
    bool is_vaild() override;

    bool allocate_2d(
        int width,
        int height,
        GLenum internal_format,
        GLenum data_format,
        GLenum data_type,
        const void* data = nullptr,
        GLenum wrap_s = GL_REPEAT,
        GLenum wrap_t = GL_REPEAT,
        GLenum min_filter = GL_LINEAR,
        GLenum mag_filter = GL_LINEAR,
        bool generate_mipmaps = false);
    void bind(GLuint texture_unit = 0) const;
    void bind_image(GLuint image_unit, GLenum access, GLenum format, GLint level = 0, GLboolean layered = GL_FALSE, GLint layer = 0) const;
    void clear(GLenum format, GLenum type, const void* data) const;
    void copy_from_framebuffer(int source_x, int source_y, int width, int height, int destination_x = 0, int destination_y = 0) const;
    GLuint get_id() const { return id_; }
    int get_width() const { return width_; }
    int get_height() const { return height_; }
    int get_channels() const { return channels_; }
};
