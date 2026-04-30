#pragma once

#include <algorithm>
#include <array>
#include <vector>

#include "compute_shader.h"
#include "fluid_bounds.h"
#include "fluid_particle.h"
#include "planet_terrain.h"
#include "planetary_water_domain.h"
#include "transformable.h"

class Camera;
class Mesh;
class shader;
class rigid_body;

enum class fluid_debug_visualization_mode : int
{
    none = 0,
    ocean_fill = 1,
    constraint = 2,
    lambda = 3,
    flow_direction = 4,
    distance_to_floor = 5,
    distance_to_water_surface = 6,
    coriolis_strength = 7,
    tidal_strength = 8,
    combined_flow = 9
};

class gpu_fluid_system_component final : public transformable
{
    struct timing_stat {
        double total_ms = 0.0;
        double max_ms = 0.0;
        unsigned int sample_count = 0u;

        void add_sample(double ms) {
            total_ms += ms;
            max_ms = std::max(max_ms, ms);
            ++sample_count;
        }

        void reset() {
            total_ms = 0.0;
            max_ms = 0.0;
            sample_count = 0u;
        }

        [[nodiscard]] double average_ms() const {
            return sample_count > 0u ? total_ms / static_cast<double>(sample_count) : 0.0;
        }
    };

    compute_shader* compute_shader_ = nullptr;
    shader* render_shader_ = nullptr;
    Mesh* render_mesh_ = nullptr;
    std::vector<fluid_particle> initial_particles_;
    fluid_bounds bounds_{};
    glm::vec3 gravity_ = glm::vec3(0.f, -9.81f, 0.f);
    float particle_size_ = 5.f;
    float interaction_radius_ = 0.42f;
    float particle_radius_ = 0.16f;
    float separation_strength_ = 0.65f;
    float near_pressure_strength_ = 0.12f;
    float velocity_damping_ = 0.35f;
    float viscosity_strength_ = 0.18f;
    float rest_density_ = 10.f;
    bool planetary_surface_enabled_ = false;
    glm::vec3 planetary_center_ = glm::vec3(0.f);
    float planetary_radius_ = 1.f;
    float planetary_shell_thickness_ = 0.1f;
    float planetary_gravity_strength_ = 9.81f;
    float planetary_water_surface_radius_ = 1.f;
    float planetary_water_coverage_ = 0.0f;
    float planetary_downslope_strength_ = 0.22f;
    float planetary_bottom_friction_ = 0.62f;
    float planetary_bottom_normal_damping_ = 0.88f;
    float planetary_floor_attraction_strength_ = 0.72f;
    float planetary_flood_guidance_strength_ = 1.25f;
    float planetary_surface_layer_thickness_scale_ = 0.42f;
    float planetary_surface_layer_attraction_strength_ = 0.78f;
    float planetary_surface_layer_normal_velocity_damping_ = 0.72f;
    std::vector<size_t> planetary_respawn_candidate_indices_;
    std::vector<glm::vec3> planetary_flood_respawn_normals_;
    std::vector<float> planetary_flood_respawn_radii_;
    planetary_water_domain planetary_water_domain_{};
    bool planetary_respawn_management_enabled_ = true;
    unsigned int planetary_respawn_interval_frames_ = 12u;
    unsigned int planetary_respawn_frame_counter_ = 0u;
    size_t planetary_respawn_scan_cursor_ = 0u;
    size_t planetary_respawn_cursor_ = 0u;
    scene_node* planetary_surface_frame_node_ = nullptr;
    glm::vec3 planetary_angular_velocity_ = glm::vec3(0.f);
    float planetary_rotation_rate_scale_ = 1.0f;
    float planetary_coriolis_strength_ = 1.0f;
    float planetary_tidal_strength_ = 1.0f;
    glm::quat previous_simulation_rotation_ = glm::quat(1.f, 0.f, 0.f, 0.f);
    bool previous_simulation_rotation_valid_ = false;
    bool planetary_terrain_enabled_ = false;
    planet_terrain::rocky_planet_profile planetary_terrain_profile_{};
    fluid_debug_visualization_mode debug_visualization_mode_ = fluid_debug_visualization_mode::none;
    bool debug_readback_enabled_ = false;
    unsigned int debug_readback_interval_frames_ = 30u;
    unsigned int debug_readback_frame_counter_ = 0u;
    unsigned int solver_substeps_ = 3;
    unsigned int constraint_iterations_ = 4;
    unsigned int runtime_solver_substeps_ = 3;
    unsigned int runtime_constraint_iterations_ = 4;
    unsigned int budget_recovery_frames_ = 0;
    GLsync gpu_completion_fence_ = 0;
    timing_stat gpu_completion_fence_wait_timing_{};
    timing_stat respawn_total_timing_{};
    timing_stat respawn_ssbo_readback_timing_{};
    timing_stat hydrology_classification_timing_{};
    timing_stat respawn_ssbo_upload_timing_{};
    mutable GLuint render_flood_mask_gpu_query_ = 0;
    mutable bool render_flood_mask_gpu_query_pending_ = false;
    mutable timing_stat render_flood_mask_gpu_timing_{};
    unsigned int runtime_timing_report_frames_ = 0u;
    float cell_size_ = 0.42f;
    bool initialized_ = false;
    size_t particle_count_ = 0;
    GLuint grid_size_x_ = 1;
    GLuint grid_size_y_ = 1;
    GLuint grid_size_z_ = 1;
    size_t cell_count_ = 1;

    static constexpr GLuint particle_binding_ = 0;
    static constexpr GLuint cell_head_binding_ = 1;
    static constexpr GLuint particle_next_binding_ = 2;
    static constexpr GLuint respawn_candidate_count_binding_ = 3;
    static constexpr GLuint respawn_candidate_indices_binding_ = 4;
    static constexpr size_t max_respawn_candidate_count_ = 2048u;
    static constexpr int planetary_flood_mask_texture_width_ = 1024;
    static constexpr int planetary_flood_mask_texture_height_ = 512;
    static constexpr int max_planetary_external_gravity_sources_ = 8;
    std::array<glm::vec4, max_planetary_external_gravity_sources_> planetary_external_gravity_sources_{};
    int planetary_external_gravity_source_count_ = 0;

    void rebuild_grid_metadata();
    void rebuild_grid_buffers();
    void ensure_initialized();
    void release_gpu_completion_fence();
    void release_render_flood_mask_gpu_query() const;
    void update_adaptive_budget();
    void queue_gpu_completion_fence();
    void update_debug_readback();
    void rebuild_planetary_water_surface_radius();
    void rebuild_planetary_flood_guidance();
    [[nodiscard]] float sample_planetary_flood_mask(const glm::vec3& surface_normal) const;
    [[nodiscard]] float sample_planetary_water_level(const glm::vec3& surface_normal) const;
    void update_planetary_particle_respawn(const glm::mat3& planetary_surface_from_simulation);
    void collect_render_flood_mask_gpu_timing() const;
    void begin_render_flood_mask_gpu_timing() const;
    void end_render_flood_mask_gpu_timing() const;
    void report_runtime_timing();
    void update_planetary_angular_velocity(const glm::mat3& world_from_simulation, float dt);
    [[nodiscard]] rigid_body* find_planetary_host_body() const;
    int gather_planetary_external_gravity_sources(const glm::mat3& world_from_simulation);

public:
    gpu_fluid_system_component(scene_node* owner,
        compute_shader* compute_shader,
        shader* render_shader,
        Mesh* render_mesh,
        std::vector<fluid_particle> particles,
        const fluid_bounds& bounds,
        const glm::vec3& gravity = glm::vec3(0.f, -9.81f, 0.f),
        float particle_size = 5.f,
        float interaction_radius = 0.42f,
        float particle_radius = 0.16f,
        float separation_strength = 0.65f,
        float near_pressure_strength = 0.12f,
        float velocity_damping = 0.35f,
        float viscosity_strength = 0.18f,
        float rest_density = 10.f,
        unsigned int solver_substeps = 3,
        unsigned int constraint_iterations = 4);

    static type_id_t type_id();
    type_id_t get_type_id() const override;

    void attach_to(scene_node* n_node) override;
    bool detach() override;

    void fixed_update(float dt);
    void draw(Camera* camera, GLuint scene_depth_texture = 0) const;
    [[nodiscard]] bool requires_scene_depth_texture() const { return planetary_surface_enabled_; }
    [[nodiscard]] bool supports_particle_surface_pass() const { return planetary_surface_enabled_; }
    void draw_particle_surface_input(Camera* camera, GLuint scene_depth_texture = 0) const;
    void draw_planetary_water_atlas_input(shader* atlas_shader, const glm::ivec2& atlas_resolution) const;
    void draw_planetary_wave_forcing_input(shader* forcing_shader, const glm::ivec2& atlas_resolution) const;

    [[nodiscard]] size_t get_particle_count() const { return particle_count_; }
    [[nodiscard]] GLuint get_ssbo_id() const;
    [[nodiscard]] const glm::vec3& get_planetary_center() const { return planetary_center_; }
    [[nodiscard]] float get_planetary_radius() const { return planetary_radius_; }
    [[nodiscard]] float get_planetary_shell_thickness() const { return planetary_shell_thickness_; }
    [[nodiscard]] float get_planetary_water_surface_radius() const { return planetary_water_surface_radius_; }
    [[nodiscard]] const glm::vec3& get_planetary_angular_velocity() const { return planetary_angular_velocity_; }
    [[nodiscard]] float get_planetary_coriolis_strength() const { return planetary_coriolis_strength_; }
    [[nodiscard]] float get_planetary_tidal_strength() const { return planetary_tidal_strength_; }
    [[nodiscard]] int get_planetary_external_gravity_source_count() const { return planetary_external_gravity_source_count_; }
    [[nodiscard]] const std::array<glm::vec4, max_planetary_external_gravity_sources_>& get_planetary_external_gravity_sources() const { return planetary_external_gravity_sources_; }
    [[nodiscard]] bool is_planetary_terrain_enabled() const { return planetary_terrain_enabled_; }
    [[nodiscard]] const planet_terrain::rocky_planet_profile& get_planetary_terrain_profile() const { return planetary_terrain_profile_; }
    [[nodiscard]] GLuint get_planetary_water_level_texture() const { return planetary_water_domain_.get_textures().water_level_texture; }
    [[nodiscard]] const planetary_water_domain& get_planetary_water_domain() const { return planetary_water_domain_; }
    [[nodiscard]] fluid_debug_visualization_mode get_debug_visualization_mode() const { return debug_visualization_mode_; }
    void set_bounds(const fluid_bounds& bounds);
    void set_planetary_surface(const glm::vec3& center, float radius, float shell_thickness, float gravity_strength);
    void set_planetary_flow_tuning(float downslope_strength, float floor_attraction_strength, float wetting_strength, float bottom_friction, float bottom_normal_damping);
    void set_planetary_flood_guidance_strength(float strength);
    void set_planetary_surface_layer_tuning(float thickness_scale, float attraction_strength, float normal_velocity_damping);
    void set_planetary_rotation_tuning(float rotation_rate_scale, float coriolis_strength, float tidal_strength);
    void set_planetary_respawn_management(bool enabled, unsigned int interval_frames = 12u);
    void set_planetary_water_coverage(float coverage);
    void set_planetary_surface_frame_node(scene_node* frame_node);
    void set_planetary_terrain_profile(const planet_terrain::rocky_planet_profile& profile);
    void set_debug_visualization_mode(fluid_debug_visualization_mode mode);
    void set_debug_readback_enabled(bool enabled, unsigned int interval_frames = 30u);

    ~gpu_fluid_system_component() override;
};
