#include "galactic_stress_scene.h"

#include "galactic_stress_test.h"

#include "Camera.h"
#include "Renderer.h"
#include "g_shape.h"

namespace {
constexpr int stress_object_count = 5000;
constexpr float camera_height = 820.f;
constexpr float camera_distance = 2350.f;
constexpr float grid_size = 5000.f;
constexpr float sun_marker_scale = 65.f;
constexpr float sun_halo_scale_multiplier = 1.08f;
}

galactic_stress_scene::galactic_stress_scene(sim::time_sim* time)
    : scene(time) {
    initialize_scene_content();
}

void galactic_stress_scene::initialize_scene_content() {
   set_simulation_speed(3600.f);

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
    auto* sun_halo_node = create_scene_node("galactic_stress_sun_halo");
    static MeshData sun_data = g_shape::generate_sphere(1.f, 24, 16);
    sun_mesh_ = assets.create_mesh(sun_data);
    sun_shader_ = assets.create_shader("stress.sun", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/sun.fs.shader");
    auto* sun_halo_shader = assets.create_shader("stress.sun.halo", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/sun_halo.fs.shader");
    sun_node->add_component<renderer>(sun_node, sun_shader_, sun_mesh_);
    auto* sun_halo_render = sun_halo_node->add_component<renderer>(sun_halo_node, sun_halo_shader, sun_mesh_);
    sun_halo_node->set_parent(sun_node, false);
    sun_node->set_global_scale(glm::vec3(sun_marker_scale));
    sun_halo_node->set_scale(glm::vec3(sun_halo_scale_multiplier));
    sun_halo_render->set_blend_mode(renderer_blend_mode::additive);
    sun_halo_render->set_depth_write_enabled(false);
    sun_halo_render->set_cull_mode(renderer_cull_mode::front);

    galactic_stress_test::initialize_stress_scene(this, planet_renderers_, stress_object_count);
}

