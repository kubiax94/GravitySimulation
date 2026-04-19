#include "galactic_stress_scene.h"

#include "galactic_simulation_test.h"

#include "Camera.h"
#include "compute_shader.h"
#include "Renderer.h"
#include "g_shape.h"
#include "gpu_particle_system_component.h"

namespace {
constexpr int stress_object_count = 25000;
constexpr float camera_height = 820.f;
constexpr float camera_distance = 2350.f;
constexpr float grid_size = 5000.f;
constexpr float sun_marker_scale = 65.f;
constexpr float particle_simulation_speed = 2500000.f;

MeshData create_particle_point_mesh() {
    MeshData data;
    Vertex vertex{};
    vertex.Position = glm::vec3(0.f);
    vertex.Normal = glm::vec3(0.f, 1.f, 0.f);
    data.vertecies.push_back(vertex);
    data.indices = { 0u };
    return data;
}
}

galactic_stress_scene::galactic_stress_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void galactic_stress_scene::initialize_scene_content() {
  auto& assets = get_asset_manager();

    auto* cam_node = create_scene_node("galactic_stress_cam");
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(glm::vec3(0.f, camera_height, camera_distance));
    cam_node->set_global_rotation(glm::vec3(-19.f, 0.f, 0.f));

    auto* grid_node = create_scene_node("galactic_stress_grid");
    static MeshData grid_data = g_shape::generate_grid_lines(128, grid_size);
    grid_mesh_ = assets.create_mesh(grid_data);
    grid_mesh_->type = MeshType::LINES;
    grid_shader_ = assets.create_shader("stress.grid", "GravitySimulation/default.vs.shader", "GravitySimulation/default.fs.shader");
    grid_node->add_component<renderer>(grid_node, grid_shader_, grid_mesh_);
    grid_node->set_global_position(glm::vec3(0.f, -0.01f, 0.f));

    auto* sun_node = create_scene_node("galactic_stress_sun_marker");
    static MeshData sun_data = g_shape::generate_sphere(1.f, 24, 16);
    sun_mesh_ = assets.create_mesh(sun_data);
    sun_shader_ = assets.create_shader("stress.sun", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/sun.fs.shader");
    sun_node->add_component<renderer>(sun_node, sun_shader_, sun_mesh_);
    sun_node->set_global_scale(glm::vec3(sun_marker_scale));

    auto* particle_node = create_scene_node("galactic_stress_particles");
    static MeshData particle_data = create_particle_point_mesh();
    particle_mesh_ = assets.create_mesh(particle_data);
    particle_mesh_->type = MeshType::POINTS;
    particle_shader_ = assets.create_shader("stress.particles", "GravitySimulation/gpu_particle_system.vs.shader", "GravitySimulation/gpu_particle_system.fs.shader");
    particle_compute_shader_ = assets.create_compute_shader("stress.compute", "GravitySimulation/gravity_defor.glsl");
    particle_node->add_component<gpu_particle_system_component>(
        particle_node,
        particle_compute_shader_,
        particle_shader_,
        particle_mesh_,
        get_unit_system(),
        simtest::create_stress_particles(stress_object_count),
        2.5f,
        particle_simulation_speed);
}
