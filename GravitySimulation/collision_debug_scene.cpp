#include "collision_debug_scene.h"

#include "Camera.h"
#include "compute_shader.h"
#include "Renderer.h"
#include "Shader.h"
#include "aabb_collider.h"
#include "collision_debug_logger_component.h"
#include "collision_layers.h"
#include "g_shape.h"
#include "rigid_body.h"

#include <glm/gtx/string_cast.hpp>

namespace {
physics_data make_body_data(const glm::vec3& position, const glm::vec3& velocity, float mass) {
    physics_data data{};
    data.position = glm::vec4(position, mass);
    data.velocity = glm::vec4(velocity, mass);
    data.accumulated_force = glm::vec4(0.f, 0.f, 0.f, mass);
    return data;
}

template <typename T>
T* find_self_component(scene_node* node) {
    if (!node)
        return nullptr;

    auto components = node->find_component<T>(search_options::include_self | search_options::first);
    return components.empty() ? nullptr : components.front();
}

MeshData create_cube_line_mesh() {
    MeshData data;
    const glm::vec3 corners[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f}
    };

    for (const auto& corner : corners) {
        Vertex vertex{};
        vertex.Position = corner;
        vertex.Normal = glm::vec3(0.f, 1.f, 0.f);
        data.vertecies.push_back(vertex);
    }

    data.indices = {
        0u, 1u, 1u, 3u, 3u, 2u, 2u, 0u,
        4u, 5u, 5u, 7u, 7u, 6u, 6u, 4u,
        0u, 4u, 1u, 5u, 2u, 6u, 3u, 7u
    };

    return data;
}
}

collision_debug_scene::collision_debug_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void collision_debug_scene::update() {
    if (debug_gravity_compute_shader_) {
        for (auto* collider_component : get_colliders()) {
            if (!collider_component || !collider_component->get_node())
                continue;

            auto* body = find_self_component<rigid_body>(collider_component->get_node());
            if (body && body->get_compute_shader_id() != debug_gravity_compute_shader_->get_id())
                body->set_compute_shader(debug_gravity_compute_shader_->get_id());
        }
    }

    static int debug_frame_counter = 0;
    ++debug_frame_counter;
    if (debug_frame_counter % 30 == 0) {
        const char* names[] = {
            "collision_dynamic_box_a",
            "collision_dynamic_box_b",
            "collision_falling_box"
        };

        std::cout << "[collision_debug][frame] colliders=" << get_colliders().size()
            << " pairs=" << get_collision_pairs().size()
            << " solidContacts=" << get_solid_collision_contacts().size() << "\n";

        for (const char* name : names) {
            auto* node = find_scene_node(name);
            if (!node) {
                std::cout << "[collision_debug][node] " << name << " missing\n";
                continue;
            }

            auto* body = node->find_component<rigid_body>();
            auto* col = node->find_component<aabb_collider>();
            const bool registered = col && std::find(get_colliders().begin(), get_colliders().end(), col) != get_colliders().end();

            std::cout << "[collision_debug][node] " << name
                << " nodePos=" << glm::to_string(node->get_global_position());

            if (body)
                std::cout << " bodyPos=" << glm::to_string(body->get_position())
                << " vel=" << glm::to_string(body->get_velocity());
            else
                std::cout << " bodyPos=<none>";

            if (col) {
                const auto bounds = col->get_world_bounds();
                std::cout << " colliderRegistered=" << (registered ? 1 : 0)
                    << " boundsMin=" << glm::to_string(bounds.min)
                    << " boundsMax=" << glm::to_string(bounds.max)
                    << " boundsValid=" << (bounds.valid ? 1 : 0);
            }
            else {
                std::cout << " colliderRegistered=0 bounds=<none>";
            }

            std::cout << "\n";
        }
    }

    scene::update();
}

void collision_debug_scene::initialize_scene_content() {
    auto& assets = get_asset_manager();
    set_simulation_speed(1.f);
    debug_gravity_compute_shader_ = assets.create_compute_shader("collision_debug.gravity", "GravitySimulation/collision_debug_gravity.glsl");
 register_compute_shader(debug_gravity_compute_shader_);

    auto* camera_node = create_scene_node("collision_debug_camera");
    camera_node->add_component<Camera>(camera_node);
    camera_node->set_global_position(glm::vec3(0.f, 18.f, 42.f));
    camera_node->set_global_rotation(glm::vec3(-18.f, 0.f, 0.f));

    auto* grid_node = create_scene_node("collision_debug_grid");
    static MeshData grid_data = g_shape::generate_grid_lines(16, 2.5f);
    auto* grid_mesh = assets.create_mesh(grid_data);
    grid_mesh->type = MeshType::LINES;
    auto* grid_shader = assets.create_shader("collision_debug.grid", "GravitySimulation/default.vs.shader", "GravitySimulation/default.fs.shader");
    auto* grid_renderer = grid_node->add_component<renderer>(grid_node, grid_shader, grid_mesh);
    grid_renderer->set_depth_write_enabled(false);
    grid_node->set_global_position(glm::vec3(0.f, 0.f, 0.f));
    grid_node->set_collision_layer(collision_layer::debug);

  static MeshData cube_mesh_data = create_cube_line_mesh();
    auto* cube_mesh = assets.create_mesh(cube_mesh_data);
    cube_mesh->type = MeshType::LINES;
    auto* cube_shader = assets.create_shader("collision_debug.cube", "GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");

    auto* floor_node = create_scene_node("collision_debug_floor");
    floor_node->set_global_position(glm::vec3(0.f, 0.75f, 0.f));
    floor_node->set_global_scale(glm::vec3(16.f, 1.5f, 16.f));
    floor_node->set_collision_layer(collision_layer::terrain);
    floor_node->set_collision_query_mask(collision_mask_all);
    auto* floor_renderer = floor_node->add_component<renderer>(floor_node, cube_shader, cube_mesh);
    floor_renderer->set_visual_scale(glm::vec3(1.f));
    auto floor_bounds = bounding_box{};
    floor_bounds.min = glm::vec3(-0.5f);
    floor_bounds.max = glm::vec3(0.5f);
    floor_bounds.valid = true;
    auto* floor_collider = floor_node->add_component<aabb_collider>(floor_node, floor_bounds);
    floor_collider->set_auto_generated(false);
    floor_node->add_component<collision_debug_logger_component>(floor_node, "collision_debug_floor");

    auto* dynamic_box_a = create_scene_node("collision_dynamic_box_a");
    dynamic_box_a->set_global_position(glm::vec3(-7.f, 2.f, 0.f));
    dynamic_box_a->set_global_scale(glm::vec3(2.2f));
    dynamic_box_a->set_collision_layer(collision_layer::character);
    dynamic_box_a->set_collision_query_mask(collision_mask_all);
    auto* dynamic_renderer_a = dynamic_box_a->add_component<renderer>(dynamic_box_a, cube_shader, cube_mesh);
    dynamic_renderer_a->set_visual_scale(glm::vec3(1.f));
   auto* dynamic_collider_a = find_self_component<aabb_collider>(dynamic_box_a);
    if (!dynamic_collider_a)
        dynamic_collider_a = dynamic_box_a->add_component<aabb_collider>(dynamic_box_a, floor_bounds);
    dynamic_collider_a->set_local_bounds(floor_bounds);
    dynamic_collider_a->set_auto_generated(false);
    auto dynamic_body_a = make_body_data(glm::vec3(-7.f, 2.f, 0.f), glm::vec3(5.5f, 0.f, 0.f), 2.0f);
   auto* dynamic_body_component_a = new rigid_body(dynamic_box_a, new physics_data(dynamic_body_a));
    dynamic_body_component_a->set_compute_shader(debug_gravity_compute_shader_->get_id());
    dynamic_box_a->add_component(dynamic_body_component_a);
    dynamic_box_a->add_component<collision_debug_logger_component>(dynamic_box_a, "collision_dynamic_box_a");

    auto* dynamic_box_b = create_scene_node("collision_dynamic_box_b");
    dynamic_box_b->set_global_position(glm::vec3(5.f, 2.f, 0.f));
    dynamic_box_b->set_global_scale(glm::vec3(2.2f));
    dynamic_box_b->set_collision_layer(collision_layer::environment);
    dynamic_box_b->set_collision_query_mask(collision_mask_all);
    auto* dynamic_renderer_b = dynamic_box_b->add_component<renderer>(dynamic_box_b, cube_shader, cube_mesh);
    dynamic_renderer_b->set_visual_scale(glm::vec3(1.f));
   auto* dynamic_collider_b = find_self_component<aabb_collider>(dynamic_box_b);
    if (!dynamic_collider_b)
        dynamic_collider_b = dynamic_box_b->add_component<aabb_collider>(dynamic_box_b, floor_bounds);
    dynamic_collider_b->set_local_bounds(floor_bounds);
    dynamic_collider_b->set_auto_generated(false);
    auto dynamic_body_b = make_body_data(glm::vec3(5.f, 2.f, 0.f), glm::vec3(-3.5f, 0.f, 0.f), 1.5f);
   auto* dynamic_body_component_b = new rigid_body(dynamic_box_b, new physics_data(dynamic_body_b));
    dynamic_body_component_b->set_compute_shader(debug_gravity_compute_shader_->get_id());
    dynamic_box_b->add_component(dynamic_body_component_b);
    dynamic_box_b->add_component<collision_debug_logger_component>(dynamic_box_b, "collision_dynamic_box_b");

    //auto* trigger_zone = create_scene_node("collision_trigger_zone");
    //trigger_zone->set_global_position(glm::vec3(0.f, 2.5f, -5.f));
    //trigger_zone->set_global_scale(glm::vec3(3.5f, 3.f, 3.5f));
    //trigger_zone->set_collision_layer(collision_layer::sensor);
    //trigger_zone->set_collision_query_mask(collision_mask_all);
    //auto* trigger_renderer = trigger_zone->add_component<renderer>(trigger_zone, cube_shader, cube_mesh);
    //trigger_renderer->set_blend_mode(renderer_blend_mode::alpha);
    //trigger_renderer->set_depth_write_enabled(false);
    //auto* trigger_collider = trigger_zone->add_component<aabb_collider>(trigger_zone, floor_bounds);
    //trigger_collider->set_trigger(true);
    //trigger_collider->set_auto_generated(false);
    //trigger_zone->add_component<collision_debug_logger_component>(trigger_zone, "collision_trigger_zone");

    auto* falling_box = create_scene_node("collision_falling_box");
    falling_box->set_global_position(glm::vec3(0.f, 8.f, 4.f));
    falling_box->set_global_scale(glm::vec3(2.f));
    falling_box->set_collision_layer(collision_layer::default_layer);
    falling_box->set_collision_query_mask(collision_mask_all);
    auto* falling_renderer = falling_box->add_component<renderer>(falling_box, cube_shader, cube_mesh);
    falling_renderer->set_visual_scale(glm::vec3(1.f));
   auto* falling_collider = find_self_component<aabb_collider>(falling_box);
    if (!falling_collider)
        falling_collider = falling_box->add_component<aabb_collider>(falling_box, floor_bounds);
    falling_collider->set_local_bounds(floor_bounds);
    falling_collider->set_auto_generated(false);
    auto falling_body = make_body_data(glm::vec3(0.f, 8.f, 4.f), glm::vec3(0.f, -1.5f, -1.5f), 1.0f);
   auto* falling_body_component = new rigid_body(falling_box, new physics_data(falling_body));
    falling_body_component->set_compute_shader(debug_gravity_compute_shader_->get_id());
    falling_box->add_component(falling_body_component);
   falling_box->add_component<collision_debug_logger_component>(falling_box, "collision_falling_box");
}
