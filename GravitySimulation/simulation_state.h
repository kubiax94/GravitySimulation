#pragma once

#include <array>
#include <memory>

#include "engine_state.h"
#include "gpu_fluid_system_component.h"
#include "loading_feedback_presenter.h"
#include "planetary_water_render_resource.h"
#include "planetary_wave_field.h"
#include "render_pipeline.h"
#include "Scene.h"
#include "Shader.h"
#include "Mesh.h"

class simulation_state : public engine_state
{
public:
    enum class example_scene_kind {
        fluid = 0,
        cloth,
        galactic,
        galactic_stress,
        collision_debug
    };

private:
    std::unique_ptr<scene> scene_;
    Camera* cam_ = nullptr;
    example_scene_kind scene_kind_ = example_scene_kind::fluid;

    render_pipeline render_pipeline_;
    planetary_water_render_resource* planetary_water_render_resource_ = nullptr;
    shader* particle_surface_composite_shader_ = nullptr;
    shader* particle_surface_blur_shader_ = nullptr;
    shader* planetary_water_atlas_shader_ = nullptr;
    shader* planetary_water_atlas_blur_shader_ = nullptr;
    shader* planetary_water_atlas_temporal_shader_ = nullptr;
    shader* planetary_water_shell_shader_ = nullptr;
    shader* planetary_wave_debug_overlay_shader_ = nullptr;
    shader* planetary_wave_debug_shell_shader_ = nullptr;
    compute_shader* planetary_wave_propagation_shader_ = nullptr;
    compute_shader* planetary_wave_render_filter_shader_ = nullptr;
    compute_shader* planetary_tide_field_shader_ = nullptr;
    Mesh* particle_surface_composite_mesh_ = nullptr;
    Mesh* planetary_water_shell_mesh_ = nullptr;
    planetary_wave_field planetary_wave_field_{};
    unsigned int planetary_wave_debug_region_texture_ = 0;
    unsigned int planetary_wave_debug_shore_texture_ = 0;
    float planetary_wave_debug_height_scale_ = 1.0f;
    float planetary_wave_debug_velocity_scale_ = 1.0f;
    float planetary_wave_debug_tidal_scale_ = 1.0f;
    shader* bounding_box_shader_ = nullptr;
    Mesh* bounding_box_mesh_ = nullptr;
    Mesh* collision_contact_mesh_ = nullptr;
    bool previous_left_mouse_down_ = false;
    bool previous_escape_down_ = false;
    bool previous_terrain_debug_down_ = false;
    bool previous_bounding_box_debug_down_ = false;
    bool previous_collision_debug_down_ = false;
  bool previous_fluid_debug_next_down_ = false;
    bool previous_fluid_debug_prev_down_ = false;
    std::array<bool, 5> previous_scene_switch_down_{};
    int terrain_debug_mode_ = 5;
 fluid_debug_visualization_mode fluid_debug_mode_ = fluid_debug_visualization_mode::none;
    bool focus_active_ = false;
    bool loading_feedback_active_ = false;
    bool draw_bounding_boxes_ = false;
    bool draw_collision_debug_ = false;
    float focus_elapsed_ = 0.f;
    float focus_duration_ = 1.8f;
    glm::vec3 focus_start_position_ = glm::vec3(0.f);
    glm::vec3 focus_target_position_ = glm::vec3(0.f);
    glm::vec3 focus_target_offset_ = glm::vec3(0.f);
    glm::vec3 focus_look_at_ = glm::vec3(0.f);
    scene_node* focus_target_node_ = nullptr;
    scene_node* attached_camera_parent_ = nullptr;
    std::unique_ptr<loading_feedback_presenter> loading_feedback_presenter_;

    void try_begin_focus();
    void update_camera_focus(float dt);
    void cancel_camera_focus();
    void detach_camera_parent();
    void switch_scene(engine& engine, example_scene_kind next_scene_kind);
    void initialize_particle_surface_composite_resources();
    void ensure_planetary_water_atlas_targets(int width, int height);
    void release_planetary_water_atlas_resources();
    void blur_planetary_water_atlas(const gpu_fluid_system_component& system);
    void stabilize_planetary_water_atlas(const gpu_fluid_system_component& system);
    void update_planetary_tide_field(const gpu_fluid_system_component& system);
    void update_planetary_wave_field(const gpu_fluid_system_component& system);
    void blur_particle_surface_targets();
    void render_planetary_water_atlas_input(const gpu_fluid_system_component& system);
    void render_planetary_water_shell(const gpu_fluid_system_component& system, const glm::vec3& light_position, const glm::vec3& light_color, float light_intensity);
    void render_planetary_wave_debug_overlay(const gpu_fluid_system_component& system);
    void render_particle_surface_composite();
    void initialize_bounding_box_debug_resources();
    void render_bounding_boxes(engine& engine);
    void render_collision_debug(engine& engine);
    void update_loading_feedback(engine& engine);

public:
    simulation_state() = default;
    explicit simulation_state(example_scene_kind scene_kind);
    explicit simulation_state(std::unique_ptr<scene> scene);

    void on_enter(engine& engine) override;
    void on_exit(engine& engine) override;
    void handle_input(engine& engine, float dt) override;
    void fixed_update(engine& engine, float dt) override;
    void update(engine& engine, float dt) override;
    void render(engine& engine) override;
  virtual std::unique_ptr<loading_feedback_presenter> create_loading_feedback_presenter(engine& engine);
};