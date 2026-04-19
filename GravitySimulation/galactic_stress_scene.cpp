#include "galactic_stress_scene.h"

#include "galactic_simulation_test.h"

#include "Camera.h"
#include "Renderer.h"
#include "g_shape.h"

namespace {
constexpr int stress_object_count = 10000;
constexpr float camera_height = 820.f;
constexpr float camera_distance = 2350.f;
constexpr float grid_size = 5000.f;
constexpr float sun_marker_scale = 65.f;
}

galactic_stress_scene::galactic_stress_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void galactic_stress_scene::initialize_scene_content() {
    auto* cam_node = create_scene_node("galactic_stress_cam");
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(glm::vec3(0.f, camera_height, camera_distance));
    cam_node->set_global_rotation(glm::vec3(-19.f, 0.f, 0.f));

    auto* grid_node = create_scene_node("galactic_stress_grid");
    static MeshData grid_data = g_shape::generate_grid_lines(128, grid_size);
    grid_mesh_ = std::make_unique<Mesh>(grid_data);
    grid_mesh_->type = MeshType::LINES;
    grid_shader_ = std::make_unique<shader>("GravitySimulation/default.vs.shader", "GravitySimulation/default.fs.shader");
    grid_node->add_component<renderer>(grid_node, grid_shader_.get(), grid_mesh_.get());
    grid_node->set_global_position(glm::vec3(0.f, -0.01f, 0.f));

    auto* sun_node = create_scene_node("galactic_stress_sun_marker");
    static MeshData sun_data = g_shape::generate_sphere(1.f, 24, 16);
    sun_mesh_ = std::make_unique<Mesh>(sun_data);
    sun_shader_ = std::make_unique<shader>("GravitySimulation/lightsource.vs.shader", "GravitySimulation/sun.fs.shader");
    sun_node->add_component<renderer>(sun_node, sun_shader_.get(), sun_mesh_.get());
    sun_node->set_global_scale(glm::vec3(sun_marker_scale));

    simtest::stress_test(this, stress_renderers_, stress_object_count);
}
