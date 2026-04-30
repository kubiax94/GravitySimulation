#pragma once

#include <memory>
#include <vector>

#include "Scene.h"
#include "planetary_wave_field.h"
#include "Renderer.h"

class Mesh;
class shader;
class compute_shader;
class planetary_water_render_resource;
class gpu_fluid_system_component;
class engine;

class galactic_scene final : public scene
{
    struct runtime_resources {
        planetary_water_render_resource* planetary_water_render_resource = nullptr;
        shader* particle_surface_composite_shader = nullptr;
        shader* particle_surface_blur_shader = nullptr;
        shader* planetary_water_atlas_shader = nullptr;
        shader* planetary_water_atlas_blur_shader = nullptr;
        shader* planetary_water_atlas_temporal_shader = nullptr;
        shader* planetary_water_shell_shader = nullptr;
        shader* planetary_wave_debug_shell_shader = nullptr;
        compute_shader* planetary_wave_propagation_shader = nullptr;
        compute_shader* planetary_wave_render_filter_shader = nullptr;
        compute_shader* planetary_tide_field_shader = nullptr;
        Mesh* particle_surface_composite_mesh = nullptr;
        Mesh* planetary_water_shell_mesh = nullptr;
        float planetary_wave_debug_height_scale = 1.0f;
        float planetary_wave_debug_velocity_scale = 1.0f;
        float planetary_wave_debug_tidal_scale = 1.0f;
    };

    std::vector<renderer*> planet_renderers_;
    scene_node* camera_node_ = nullptr;
    scene_node* background_star_node_ = nullptr;
    scene_node* background_galaxy_node_ = nullptr;
    render_pipeline* active_render_pipeline_ = nullptr;
    runtime_resources runtime_resources_{};
    planetary_wave_field planetary_wave_field_{};

    void initialize_scene_content();
    void ensure_planetary_water_atlas_targets(int width, int height);
    void release_planetary_water_atlas_resources();
    void blur_planetary_water_atlas(const gpu_fluid_system_component& system);
    void stabilize_planetary_water_atlas(const gpu_fluid_system_component& system);
    void update_planetary_tide_field(const gpu_fluid_system_component& system, int debug_mode);
    void update_planetary_wave_field(const gpu_fluid_system_component& system, int debug_mode);
    void render_planetary_water_atlas_input(const gpu_fluid_system_component& system);
    void render_planetary_water_shell(const scene_render_context& context, const gpu_fluid_system_component& system) const;
    void render_planetary_wave_debug_overlay(const scene_render_context& context, const gpu_fluid_system_component& system) const;

public:
    explicit galactic_scene(sim::time* time);
    void initialize_runtime_resources() override;
    void release_runtime_resources() override;
    [[nodiscard]] bool render_fluid_system(engine& engine, const scene_render_context& context, const gpu_fluid_system_component& system) override;
    [[nodiscard]] bool render_runtime(engine& engine, const scene_render_context& context) override;
    void update() override;
    [[nodiscard]] bool has_primary_light() const override { return true; }
    [[nodiscard]] glm::vec3 get_primary_light_position() const override { return glm::vec3(0.f); }
    [[nodiscard]] glm::vec3 get_primary_light_color() const override { return glm::vec3(1.0f, 0.82f, 0.45f); }
    [[nodiscard]] float get_primary_light_intensity() const override { return 1.2f; }
    ~galactic_scene() override = default;
};
