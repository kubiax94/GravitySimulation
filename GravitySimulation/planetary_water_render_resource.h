#pragma once

#include "asset.h"
#include "texture.h"

class planetary_water_render_resource : public asset
{
public:
    struct atlas_targets {
        texture atlas_texture;
        texture atlas_ping_texture;
        texture atlas_history_texture;
        texture wave_forcing_texture;
        texture wave_forcing_ping_texture;
        texture wave_forcing_history_texture;
        texture tide_height_texture;
        GLuint framebuffer = 0;
        int width = 0;
        int height = 0;
    };

private:
    atlas_targets atlas_targets_{};

public:
    explicit planetary_water_render_resource(const std::string& name = "");
    ~planetary_water_render_resource() override = default;

    bool load() override;
    bool finalize() override;
    void unload() override;
    void cleanup() override;
    bool is_vaild() override;

    bool ensure_atlas_targets(int width, int height);
    void release_atlas_targets();

    atlas_targets& get_atlas_targets() { return atlas_targets_; }
    const atlas_targets& get_atlas_targets() const { return atlas_targets_; }
};
