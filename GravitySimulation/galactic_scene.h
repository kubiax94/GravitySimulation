#pragma once

#include <memory>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "asset.h"
#include "Scene.h"
#include "planetary_wave_field.h"
#include "render_pipeline.h"
#include "Renderer.h"

class Mesh;
class shader;
class compute_shader;
class planetary_water_render_resource;
class gpu_fluid_system_component;
class engine;

class galactic_scene final : public scene
{
    enum class runtime_resource_key : std::uint8_t {
        planetary_water_render,
        particle_surface_composite_shader,
        particle_surface_blur_shader,
        planetary_water_atlas_shader,
        planetary_water_atlas_blur_shader,
        planetary_water_atlas_temporal_shader,
        planetary_water_shell_shader,
        planetary_wave_debug_shell_shader,
        planetary_wave_propagation_shader,
        planetary_wave_render_filter_shader,
        planetary_tide_field_shader,
        particle_surface_composite_mesh,
        planetary_water_shell_mesh
    };

    struct runtime_resource_key_hash {
        size_t operator()(runtime_resource_key key) const {
            return static_cast<size_t>(key);
        }
    };

    struct runtime_resources {
        std::unordered_map<runtime_resource_key, asset*, runtime_resource_key_hash> assets;
        float planetary_wave_debug_height_scale = 1.0f;
        float planetary_wave_debug_velocity_scale = 1.0f;
        float planetary_wave_debug_tidal_scale = 1.0f;

        template<typename T>
        T* get(runtime_resource_key key) const {
            const auto it = assets.find(key);
            if (it == assets.end())
                return nullptr;

            return dynamic_cast<T*>(it->second);
        }

        template<typename T>
        void set(runtime_resource_key key, T* value) {
            assets[key] = value;
        }

        void clear() {
            assets.clear();
        }
    };

    std::vector<renderer*> planet_renderers_;
    scene_node* camera_node_ = nullptr;
    scene_node* background_star_node_ = nullptr;
    scene_node* background_galaxy_node_ = nullptr;
    render_pipeline* active_render_pipeline_ = nullptr;
    runtime_resources runtime_resources_{};
    planetary_wave_field planetary_wave_field_{};
    int wave_debug_mode_ = 0;
    bool previous_wave_debug_next_down_ = false;
    bool previous_wave_debug_prev_down_ = false;

    void initialize_scene_content();
    planetary_water_render_resource* get_water_render_resource() const;
    shader* get_particle_surface_composite_shader() const;
    Mesh* get_particle_surface_composite_mesh() const;
    shader* get_water_atlas_shader() const;
    shader* get_water_atlas_blur_shader() const;
    shader* get_water_atlas_temporal_shader() const;
    shader* get_water_shell_shader() const;
    shader* get_wave_debug_shell_shader() const;
    compute_shader* get_wave_propagation_shader() const;
    compute_shader* get_wave_render_filter_shader() const;
    compute_shader* get_tide_field_shader() const;
    Mesh* get_water_shell_mesh() const;
    void ensure_planetary_water_atlas_targets(int width, int height);
    void release_planetary_water_atlas_resources();
    void blur_planetary_water_atlas(const gpu_fluid_system_component& system);
    void stabilize_planetary_water_atlas(const gpu_fluid_system_component& system);
    void update_planetary_tide_field(const gpu_fluid_system_component& system, int debug_mode);
    void update_planetary_wave_field(const gpu_fluid_system_component& system, int debug_mode);
    void render_planetary_water_atlas_input(const gpu_fluid_system_component& system);
    void apply_planetary_shell_common_uniforms(shader& target_shader, const scene_render_context& context, const gpu_fluid_system_component& system) const;
    std::vector<render_pipeline::texture_binding> build_planetary_shell_texture_bindings(const gpu_fluid_system_component& system) const;
    void render_planetary_water_shell(const scene_render_context& context, const gpu_fluid_system_component& system) const;
    void render_planetary_wave_debug_overlay(const scene_render_context& context, const gpu_fluid_system_component& system) const;

public:
    explicit galactic_scene(sim::time_sim* time);
    void initialize_runtime_resources() override;
    void release_runtime_resources() override;
    void handle_input(engine& engine, float dt) override;
    [[nodiscard]] int get_wave_debug_mode() const { return wave_debug_mode_; }
    [[nodiscard]] bool render_fluid_system(engine& engine, const scene_render_context& context, const gpu_fluid_system_component& system) override;
    [[nodiscard]] bool render_runtime(engine& engine, const scene_render_context& context) override;
    void update() override;
    [[nodiscard]] bool has_primary_light() const override { return true; }
    [[nodiscard]] glm::vec3 get_primary_light_position() const override { return glm::vec3(0.f); }
    [[nodiscard]] glm::vec3 get_primary_light_color() const override { return glm::vec3(1.0f, 0.82f, 0.45f); }
    [[nodiscard]] float get_primary_light_intensity() const override { return 1.2f; }
    ~galactic_scene() override = default;
};

