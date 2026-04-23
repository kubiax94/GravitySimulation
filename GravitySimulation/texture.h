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
    std::vector<unsigned char> pixel_data_;

public:
    explicit texture(const std::string& texture_path = "", const std::string& name = "");
    ~texture() override = default;

    bool load() override;
    bool finalize() override;
    void unload() override;
    void cleanup() override;
    bool is_vaild() override;

    void bind(GLuint texture_unit = 0) const;
    GLuint get_id() const { return id_; }
    int get_width() const { return width_; }
    int get_height() const { return height_; }
    int get_channels() const { return channels_; }
};
