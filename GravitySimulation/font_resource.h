#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "asset.h"
#include "texture.h"

class font_resource final : public asset
{
public:
    struct glyph_info {
        glm::vec2 uv_min = glm::vec2(0.0f);
        glm::vec2 uv_max = glm::vec2(0.0f);
        glm::ivec2 size = glm::ivec2(0);
        glm::ivec2 bearing = glm::ivec2(0);
        unsigned int advance = 0;
        bool valid = false;
    };

private:
    struct pending_glyph {
        std::uint32_t codepoint = 0;
        glyph_info glyph;
        int atlas_x = 0;
        int atlas_y = 0;
        std::vector<unsigned char> bitmap;
    };

    texture atlas_texture_;
    std::unordered_map<std::uint32_t, glyph_info> glyphs_;
    std::vector<unsigned char> atlas_pixels_;
    std::uint32_t first_codepoint_ = 32u;
    std::uint32_t last_codepoint_ = 126u;
    unsigned int pixel_height_ = 32u;
    int atlas_width_ = 0;
    int atlas_height_ = 0;
    int line_height_ = 0;
    int ascender_ = 0;
    int descender_ = 0;

    static std::string resolve_font_path(const std::string& path) {
        if (path.empty())
            return {};

        std::filesystem::path input(path);
        if (std::filesystem::exists(input))
            return input.string();

        auto cwd = std::filesystem::current_path();
        for (int i = 0; i < 6; ++i) {
            const auto candidate = cwd / input;
            if (std::filesystem::exists(candidate))
                return candidate.string();
            if (!cwd.has_parent_path())
                break;
            cwd = cwd.parent_path();
        }

        return {};
    }

    void clear_loaded_data() {
        glyphs_.clear();
        atlas_pixels_.clear();
        atlas_pixels_.shrink_to_fit();
        atlas_width_ = 0;
        atlas_height_ = 0;
        line_height_ = 0;
        ascender_ = 0;
        descender_ = 0;
    }

public:
    explicit font_resource(
        const std::string& font_path = "",
        const std::string& name = "",
        unsigned int pixel_height = 32u,
        std::uint32_t first_codepoint = 32u,
        std::uint32_t last_codepoint = 126u)
        : asset(asset_type::FONT, name),
          atlas_texture_("", name + ".atlas"),
          first_codepoint_(first_codepoint),
          last_codepoint_(std::max(first_codepoint, last_codepoint)),
          pixel_height_(std::max(1u, pixel_height)) {
        set_source_path(font_path);
    }

    bool load() override {
        cleanup();

        const std::string resolved_path = resolve_font_path(get_source_path());
        if (resolved_path.empty()) {
            std::cerr << "ERROR::FONT::FILE_NOT_FOUND: " << get_source_path() << std::endl;
            return false;
        }

        FT_Library library = nullptr;
        if (FT_Init_FreeType(&library) != 0) {
            std::cerr << "ERROR::FONT::FREETYPE_INIT_FAILED" << std::endl;
            return false;
        }

        FT_Face face = nullptr;
        if (FT_New_Face(library, resolved_path.c_str(), 0, &face) != 0) {
            std::cerr << "ERROR::FONT::FACE_LOAD_FAILED: " << resolved_path << std::endl;
            FT_Done_FreeType(library);
            return false;
        }

        if (FT_Set_Pixel_Sizes(face, 0, pixel_height_) != 0) {
            std::cerr << "ERROR::FONT::SET_PIXEL_SIZE_FAILED" << std::endl;
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return false;
        }

        line_height_ = static_cast<int>(face->size->metrics.height >> 6);
        ascender_ = static_cast<int>(face->size->metrics.ascender >> 6);
        descender_ = static_cast<int>(face->size->metrics.descender >> 6);

        constexpr int padding = 1;
        int max_row_width = 1024;
        int current_x = padding;
        int current_y = padding;
        int row_height = 0;
        atlas_width_ = padding;
        atlas_height_ = padding;

        std::vector<pending_glyph> pending_glyphs;
        pending_glyphs.reserve(static_cast<size_t>(last_codepoint_ - first_codepoint_ + 1u));

        for (std::uint32_t codepoint = first_codepoint_; codepoint <= last_codepoint_; ++codepoint) {
            if (FT_Load_Char(face, static_cast<FT_ULong>(codepoint), FT_LOAD_RENDER) != 0)
                continue;

            const FT_GlyphSlot glyph_slot = face->glyph;
            const int bitmap_width = static_cast<int>(glyph_slot->bitmap.width);
            const int bitmap_height = static_cast<int>(glyph_slot->bitmap.rows);
            max_row_width = std::max(max_row_width, bitmap_width + padding * 2);

            if (current_x + bitmap_width + padding > max_row_width && current_x > padding) {
                current_y += row_height + padding;
                current_x = padding;
                row_height = 0;
            }

            pending_glyph glyph_entry;
            glyph_entry.codepoint = codepoint;
            glyph_entry.atlas_x = current_x;
            glyph_entry.atlas_y = current_y;
            glyph_entry.glyph.size = glm::ivec2(bitmap_width, bitmap_height);
            glyph_entry.glyph.bearing = glm::ivec2(glyph_slot->bitmap_left, glyph_slot->bitmap_top);
            glyph_entry.glyph.advance = static_cast<unsigned int>(glyph_slot->advance.x >> 6);
            glyph_entry.glyph.valid = true;

            if (bitmap_width > 0 && bitmap_height > 0) {
                glyph_entry.bitmap.resize(static_cast<size_t>(bitmap_width) * static_cast<size_t>(bitmap_height));
                for (int row = 0; row < bitmap_height; ++row) {
                    const auto* source_row = glyph_slot->bitmap.buffer + static_cast<size_t>(row) * static_cast<size_t>(glyph_slot->bitmap.pitch);
                    auto* destination_row = glyph_entry.bitmap.data() + static_cast<size_t>(row) * static_cast<size_t>(bitmap_width);
                    std::copy_n(source_row, bitmap_width, destination_row);
                }
            }

            pending_glyphs.push_back(std::move(glyph_entry));

            current_x += bitmap_width + padding;
            row_height = std::max(row_height, bitmap_height);
            atlas_width_ = std::max(atlas_width_, current_x);
            atlas_height_ = std::max(atlas_height_, current_y + row_height + padding);
        }

        FT_Done_Face(face);
        FT_Done_FreeType(library);

        if (pending_glyphs.empty() || atlas_width_ <= 0 || atlas_height_ <= 0)
            return false;

        atlas_pixels_.assign(static_cast<size_t>(atlas_width_) * static_cast<size_t>(atlas_height_), 0u);
        glyphs_.reserve(pending_glyphs.size());

        const glm::vec2 atlas_size = glm::vec2(static_cast<float>(atlas_width_), static_cast<float>(atlas_height_));
        for (const auto& pending : pending_glyphs) {
            if (!pending.bitmap.empty()) {
                for (int row = 0; row < pending.glyph.size.y; ++row) {
                    auto* destination_row = atlas_pixels_.data()
                        + static_cast<size_t>(pending.atlas_y + row) * static_cast<size_t>(atlas_width_)
                        + static_cast<size_t>(pending.atlas_x);
                    const auto* source_row = pending.bitmap.data() + static_cast<size_t>(row) * static_cast<size_t>(pending.glyph.size.x);
                    std::copy_n(source_row, static_cast<size_t>(pending.glyph.size.x), destination_row);
                }
            }

            glyph_info glyph = pending.glyph;
            glyph.uv_min = glm::vec2(static_cast<float>(pending.atlas_x), static_cast<float>(pending.atlas_y)) / atlas_size;
            glyph.uv_max = glm::vec2(
                static_cast<float>(pending.atlas_x + pending.glyph.size.x),
                static_cast<float>(pending.atlas_y + pending.glyph.size.y)) / atlas_size;
            glyphs_[pending.codepoint] = glyph;
        }

        status_ = asset_status::LOADED;
        return true;
    }

    bool finalize() override {
        if (atlas_pixels_.empty() || atlas_width_ <= 0 || atlas_height_ <= 0)
            return false;

        if (!atlas_texture_.allocate_2d(
            atlas_width_,
            atlas_height_,
            GL_R8,
            GL_RED,
            GL_UNSIGNED_BYTE,
            atlas_pixels_.data(),
            GL_CLAMP_TO_EDGE,
            GL_CLAMP_TO_EDGE,
            GL_LINEAR,
            GL_LINEAR,
            false)) {
            return false;
        }

        atlas_pixels_.clear();
        atlas_pixels_.shrink_to_fit();
        status_ = asset_status::LOADED;
        return true;
    }

    void unload() override {
        cleanup();
        resource::unload();
    }

    void cleanup() override {
        atlas_texture_.cleanup();
        clear_loaded_data();
    }

    bool is_vaild() override {
        return atlas_texture_.is_vaild() && !glyphs_.empty();
    }

    const glyph_info* find_glyph(std::uint32_t codepoint) const {
        const auto it = glyphs_.find(codepoint);
        return it != glyphs_.end() ? &it->second : nullptr;
    }

    const std::unordered_map<std::uint32_t, glyph_info>& get_glyphs() const { return glyphs_; }
    const texture& get_atlas_texture() const { return atlas_texture_; }
    texture& get_atlas_texture() { return atlas_texture_; }
    int get_atlas_width() const { return atlas_width_; }
    int get_atlas_height() const { return atlas_height_; }
    int get_line_height() const { return line_height_; }
    int get_ascender() const { return ascender_; }
    int get_descender() const { return descender_; }
    unsigned int get_pixel_height() const { return pixel_height_; }
    std::uint32_t get_first_codepoint() const { return first_codepoint_; }
    std::uint32_t get_last_codepoint() const { return last_codepoint_; }
};
