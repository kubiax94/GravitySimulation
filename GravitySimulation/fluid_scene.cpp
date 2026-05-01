#include "fluid_scene.h"

#include "Camera.h"
#include "compute_shader.h"
#include "fluid_bounds.h"
#include "fluid_particle.h"
#include "g_shape.h"
#include "gpu_fluid_system_component.h"
#include "Renderer.h"
#include "input_system.h"

#include <cmath>

namespace {
constexpr int fluid_particles_x = 32;
constexpr int fluid_particles_y = 32;
constexpr int fluid_particles_z = 32;
constexpr float particle_spacing = 0.38f;
constexpr float particle_size = .2f;
constexpr float camera_height = 10.f;
constexpr float camera_distance = 24.f;

MeshData create_particle_point_mesh() {
    MeshData data;
    Vertex vertex{};
    vertex.Position = glm::vec3(0.f);
    vertex.Normal = glm::vec3(0.f, 1.f, 0.f);
    data.vertecies.push_back(vertex);
    data.indices = { 0u };
    return data;
}

std::vector<fluid_particle> create_box_particles() {
    std::vector<fluid_particle> particles;
    particles.reserve(fluid_particles_x * fluid_particles_y * fluid_particles_z);

    const glm::vec3 base(-3.0f, 1.0f, -3.0f);
    for (int z = 0; z < fluid_particles_z; ++z) {
        for (int y = 0; y < fluid_particles_y; ++y) {
            for (int x = 0; x < fluid_particles_x; ++x) {
                fluid_particle particle;
                particle.position = glm::vec4(base + glm::vec3(x, y, z) * particle_spacing, 1.0f);
                particle.velocity = glm::vec4(0.f);
                particle.predicted_position = particle.position;
                particles.push_back(particle);
            }
        }
    }

    return particles;
}
}

fluid_scene::fluid_scene(sim::time* time)
    : scene(time), sim_time_(time) {
    initialize_scene_content();
}

void fluid_scene::update() {
    if (fluid_node_ && sim_time_) {
        const float t = sim_time_->current;
        fluid_node_->set_global_rotation(glm::vec3(
            -6.0f + std::sin(t * 0.31f) * 7.0f,
            std::sin(t * 0.17f) * 10.0f,
            std::sin(t * 0.63f) * 22.0f));
    }

    scene::update();
}

void fluid_scene::initialize_scene_content() {
    auto& assets = get_asset_manager();

    auto* cam_node = create_scene_node("fluid_cam");
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(glm::vec3(0.f, camera_height, camera_distance));
    cam_node->set_global_rotation(glm::vec3(-18.f, 0.f, 0.f));

    auto* grid_node = create_scene_node("fluid_grid");
    static MeshData grid_data = g_shape::generate_grid_lines(48, 30.f);
    grid_mesh_ = assets.create_mesh(grid_data);
    grid_mesh_->type = MeshType::LINES;
    grid_shader_ = assets.create_shader("fluid.grid", "GravitySimulation/default.vs.shader", "GravitySimulation/default.fs.shader");
    grid_node->add_component<renderer>(grid_node, grid_shader_, grid_mesh_);
    grid_node->set_global_position(glm::vec3(0.f, 0.f, 0.f));

    fluid_node_ = create_scene_node("fluid_box");
    static MeshData particle_data = create_particle_point_mesh();
    fluid_mesh_ = assets.create_mesh(particle_data);
    fluid_mesh_->type = MeshType::POINTS;
    fluid_shader_ = assets.create_shader("fluid.points", "GravitySimulation/gpu_fluid_system.vs.shader", "GravitySimulation/gpu_fluid_system_surface.fs.shader");
    fluid_compute_shader_ = assets.create_compute_shader("fluid.predict", "GravitySimulation/fluid_predict.glsl");

    fluid_bounds bounds;
    bounds.min = glm::vec3(-6.f, 0.25f, -6.f);
    bounds.max = glm::vec3(6.f, 14.f, 6.f);
    bounds.restitution = 0.18f;
    bounds.damping = 0.98f;

    fluid_system_ = fluid_node_->add_component<gpu_fluid_system_component>(
        fluid_node_,
        fluid_compute_shader_,
        fluid_shader_,
        fluid_mesh_,
        create_box_particles(),
        bounds,
        glm::vec3(0.f, -18.f, 0.f),
        particle_size,
        0.52f,
        0.17f,
        0.32f,
        0.18f,
        0.45f,
        0.22f,
        6.0f,
        3u,
        5u);
    fluid_system_->set_debug_visualization_mode(fluid_debug_visualization_mode::flow_direction);
    fluid_system_->set_debug_readback_enabled(true, 20u);

    fluid_node_->set_global_position(glm::vec3(0.f));
}

void fluid_scene::handle_input(engine& engine, float dt) {
    (void)engine;
    (void)dt;

    if (!fluid_system_)
        return;

    bool debug_mode_changed = false;
    const bool prev_down = input_system::is_key_down(GLFW_KEY_H);
    if (prev_down && !previous_debug_prev_down_) {
        auto mode = static_cast<int>(fluid_system_->get_debug_visualization_mode());
        mode = (mode + 10 - 1) % 10;
        fluid_system_->set_debug_visualization_mode(static_cast<fluid_debug_visualization_mode>(mode));
        debug_mode_changed = true;
    }
    previous_debug_prev_down_ = prev_down;

    const bool next_down = input_system::is_key_down(GLFW_KEY_J);
    if (next_down && !previous_debug_next_down_) {
        auto mode = static_cast<int>(fluid_system_->get_debug_visualization_mode());
        mode = (mode + 1) % 10;
        fluid_system_->set_debug_visualization_mode(static_cast<fluid_debug_visualization_mode>(mode));
        debug_mode_changed = true;
    }
    previous_debug_next_down_ = next_down;

    if (debug_mode_changed) {
        std::cout << "[fluid_debug_mode] " << static_cast<int>(fluid_system_->get_debug_visualization_mode()) << std::endl;
    }
}
