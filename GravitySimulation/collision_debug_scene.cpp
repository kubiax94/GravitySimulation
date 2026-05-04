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

bounding_box make_unit_cube_bounds() {
    bounding_box bounds;
    bounds.min = glm::vec3(-0.5f);
    bounds.max = glm::vec3(0.5f);
    bounds.valid = true;
    return bounds;
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

collision_debug_scene::collision_debug_scene(sim::time_sim* time)
    : scene(time), debug_time_(time) {
    initialize_scene_content();
}

scene_node* collision_debug_scene::create_static_debug_box(const std::string& name, const glm::vec3& position, const glm::vec3& scale, collision_layer layer) {
    auto* node = create_scene_node(name);
    node->set_global_position(position);
    node->set_global_scale(scale);
    node->set_collision_layer(layer);
    node->set_collision_query_mask(collision_mask_all);

    auto* box_renderer = node->add_component<renderer>(node, debug_cube_shader_, debug_cube_mesh_);
    box_renderer->set_visual_scale(glm::vec3(1.f));

    auto* collider_component = find_self_component<aabb_collider>(node);
    if (!collider_component)
        collider_component = node->add_component<aabb_collider>(node, debug_cube_bounds_);
    collider_component->set_local_bounds(debug_cube_bounds_);
    collider_component->set_auto_generated(false);

    node->add_component<collision_debug_logger_component>(node, name);
    return node;
}

scene_node* collision_debug_scene::create_dynamic_debug_box(const std::string& name, const glm::vec3& position, const glm::vec3& scale,
    const glm::vec3& velocity, float mass, collision_layer layer) {
    auto* node = create_scene_node(name);
    node->set_global_position(position);
    node->set_global_scale(scale);
    node->set_collision_layer(layer);
    node->set_collision_query_mask(collision_mask_all);

    auto* box_renderer = node->add_component<renderer>(node, debug_cube_shader_, debug_cube_mesh_);
    box_renderer->set_visual_scale(glm::vec3(1.f));

    auto* collider_component = find_self_component<aabb_collider>(node);
    if (!collider_component)
        collider_component = node->add_component<aabb_collider>(node, debug_cube_bounds_);
    collider_component->set_local_bounds(debug_cube_bounds_);
    collider_component->set_auto_generated(false);

    auto body_data = make_body_data(position, velocity, mass);
    auto* body_component = new rigid_body(node, new physics_data(body_data));
    body_component->set_compute_shader(debug_gravity_compute_shader_->get_id());
    node->add_component(body_component);
    node->add_component<collision_debug_logger_component>(node, name);
    return node;
}

void collision_debug_scene::spawn_collision_wave() {
    for (int lane_index = 0; lane_index < 4; ++lane_index) {
        const float z = -10.5f + static_cast<float>(lane_index) * 7.0f;
        const float y = 14.0f + static_cast<float>(lane_index % 2) * 2.25f;
        const float left_mass = 0.9f + static_cast<float>(lane_index) * 0.2f;
        const float right_mass = 1.1f + static_cast<float>(lane_index) * 0.2f;

        create_dynamic_debug_box("collision_wave_left_" + std::to_string(dynamic_spawn_serial_++),
            glm::vec3(-11.5f, y, z), glm::vec3(1.5f), glm::vec3(6.5f, -1.5f, 0.f), left_mass, collision_layer::character);
        create_dynamic_debug_box("collision_wave_right_" + std::to_string(dynamic_spawn_serial_++),
            glm::vec3(11.5f, y + 0.75f, z), glm::vec3(1.5f), glm::vec3(-6.0f, -1.5f, 0.f), right_mass, collision_layer::environment);
    }
}

void collision_debug_scene::spawn_crusher_wave() {
    for (int column = 0; column < 3; ++column) {
        const float z = -6.f + static_cast<float>(column) * 6.f;
        create_dynamic_debug_box("collision_crusher_left_" + std::to_string(dynamic_spawn_serial_++),
            glm::vec3(-12.f, 6.0f + static_cast<float>(column), z), glm::vec3(2.2f, 2.2f, 2.2f), glm::vec3(9.5f, -0.5f, 0.f), 2.8f, collision_layer::character);
        create_dynamic_debug_box("collision_crusher_right_" + std::to_string(dynamic_spawn_serial_++),
            glm::vec3(12.f, 6.5f + static_cast<float>(column), z + 1.5f), glm::vec3(2.2f, 2.2f, 2.2f), glm::vec3(-9.0f, -0.5f, 0.f), 2.8f, collision_layer::environment);
    }

    for (int rain_index = 0; rain_index < 4; ++rain_index) {
        const float x = -6.f + static_cast<float>(rain_index) * 4.f;
        const float z = 6.f - static_cast<float>(rain_index % 2) * 6.f;
        create_dynamic_debug_box("collision_crusher_rain_" + std::to_string(dynamic_spawn_serial_++),
            glm::vec3(x, 18.f + static_cast<float>(rain_index), z), glm::vec3(1.8f), glm::vec3((static_cast<float>(rain_index) - 1.5f) * 0.6f, -4.0f, 0.f), 1.4f, collision_layer::default_layer);
    }
}

void collision_debug_scene::update() {
    constexpr int debug_frame_log_interval = 120;
    constexpr size_t debug_manifold_log_limit = 8;

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
    if (debug_frame_counter % debug_frame_log_interval == 0) {
        const char* names[] = {
            "collision_dynamic_box_a",
            "collision_dynamic_box_b",
            "collision_pusher_box",
            "collision_stack_box_0",
            "collision_stack_box_3",
            "collision_falling_box"
        };

        std::cout << "[collision_debug][frame] colliders=" << get_colliders().size()
            << " pairs=" << get_collision_pairs().size()
            << " manifolds=" << get_contact_manifolds().size()
            << " solidContacts=" << get_solid_collision_contacts().size()
            << " cachedManifolds=" << get_cached_contact_manifold_count()
            << " persistentManifolds=" << get_persistent_contact_manifold_count()
            << " warmPoints=" << get_warm_contact_point_count()
            << " maxPersistence=" << get_max_contact_persistence() << "\n";

        size_t manifold_log_count = 0;
        for (const auto& manifold : get_contact_manifolds()) {
            if (manifold_log_count >= debug_manifold_log_limit)
                break;
            if (!manifold.first || !manifold.second)
                continue;

            auto* first_node = manifold.first->get_node();
            auto* second_node = manifold.second->get_node();
            std::cout << "[collision_debug][manifold] first=";
            if (first_node)
                std::cout << first_node->get_id().to_string();
            else
                std::cout << "null";

            std::cout << " second=";
            if (second_node)
                std::cout << second_node->get_id().to_string();
            else
                std::cout << "null";

            std::cout << " trigger=" << (manifold.is_trigger ? 1 : 0)
                << " persistence=" << manifold.persistence
                << " normal=" << glm::to_string(manifold.normal)
                << " overlapMin=" << glm::to_string(manifold.overlap_bounds.min)
                << " overlapMax=" << glm::to_string(manifold.overlap_bounds.max)
                << " pointCount=" << manifold.point_count;

            for (uint32_t point_index = 0; point_index < manifold.point_count && point_index < manifold.points.size(); ++point_index) {
                const auto& point = manifold.points[point_index];
                std::cout << " p" << point_index
                    << "Pos=" << glm::to_string(point.position)
                    << " pen=" << point.penetration
                    << " nImpulse=" << point.normal_impulse_accumulated
                    << " tImpulse=" << point.tangent_impulse_accumulated;
            }

            std::cout << "\n";
            ++manifold_log_count;
        }

        if (get_contact_manifolds().size() > manifold_log_count)
            std::cout << "[collision_debug][manifold] omitted=" << (get_contact_manifolds().size() - manifold_log_count) << "\n";

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
    camera_node->set_global_position(glm::vec3(0.f, 22.f, 34.f));
    camera_node->set_global_rotation(glm::vec3(-24.f, 0.f, 0.f));

    auto* grid_node = create_scene_node("collision_debug_grid");
    static MeshData grid_data = g_shape::generate_grid_lines(20, 2.0f);
    auto* grid_mesh = assets.create_mesh(grid_data);
    grid_mesh->type = MeshType::LINES;
    auto* grid_shader = assets.create_shader("collision_debug.grid", "GravitySimulation/default.vs.shader", "GravitySimulation/default.fs.shader");
    auto* grid_renderer = grid_node->add_component<renderer>(grid_node, grid_shader, grid_mesh);
    grid_renderer->set_depth_write_enabled(false);
    grid_node->set_global_position(glm::vec3(0.f, 0.f, 0.f));
    grid_node->set_collision_layer(collision_layer::debug);

    static MeshData cube_mesh_data = create_cube_line_mesh();
    debug_cube_mesh_ = assets.create_mesh(cube_mesh_data);
    debug_cube_mesh_->type = MeshType::LINES;
    debug_cube_shader_ = assets.create_shader("collision_debug.cube", "GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");
    debug_cube_bounds_ = make_unit_cube_bounds();

    create_static_debug_box("collision_debug_floor", glm::vec3(0.f, 0.5f, 0.f), glm::vec3(28.f, 1.f, 28.f), collision_layer::terrain);
    create_static_debug_box("collision_wall_west", glm::vec3(-14.5f, 4.f, 0.f), glm::vec3(1.f, 8.f, 28.f), collision_layer::terrain);
    create_static_debug_box("collision_wall_east", glm::vec3(14.5f, 4.f, 0.f), glm::vec3(1.f, 8.f, 28.f), collision_layer::terrain);
    create_static_debug_box("collision_wall_north", glm::vec3(0.f, 4.f, -14.5f), glm::vec3(28.f, 8.f, 1.f), collision_layer::terrain);
    create_static_debug_box("collision_wall_south", glm::vec3(0.f, 4.f, 14.5f), glm::vec3(28.f, 8.f, 1.f), collision_layer::terrain);

    create_dynamic_debug_box("collision_dynamic_box_a", glm::vec3(-10.f, 2.2f, -4.f), glm::vec3(2.f), glm::vec3(6.5f, 0.f, 0.f), 2.0f, collision_layer::character);
    create_dynamic_debug_box("collision_dynamic_box_b", glm::vec3(10.f, 2.2f, -4.f), glm::vec3(2.f), glm::vec3(-4.5f, 0.f, 0.f), 1.5f, collision_layer::environment);
    create_dynamic_debug_box("collision_pusher_box", glm::vec3(-11.f, 2.2f, 4.f), glm::vec3(2.f), glm::vec3(8.0f, 0.f, 0.f), 2.5f, collision_layer::character);

    for (int stack_index = 0; stack_index < 4; ++stack_index) {
        const std::string name = "collision_stack_box_" + std::to_string(stack_index);
        create_dynamic_debug_box(name, glm::vec3(0.f, 2.2f + static_cast<float>(stack_index) * 2.05f, 4.f), glm::vec3(2.f), glm::vec3(0.f), 1.2f, collision_layer::default_layer);
    }

    create_dynamic_debug_box("collision_falling_box", glm::vec3(0.f, 12.f, 4.f), glm::vec3(2.f), glm::vec3(0.f, -2.0f, 0.f), 1.0f, collision_layer::default_layer);

    for (int lane_index = 0; lane_index < 4; ++lane_index) {
        const float z = -11.f + static_cast<float>(lane_index) * 3.5f;
        const std::string left_name = "collision_lane_left_" + std::to_string(lane_index);
        const std::string right_name = "collision_lane_right_" + std::to_string(lane_index);
        create_dynamic_debug_box(left_name, glm::vec3(-11.5f, 2.2f, z), glm::vec3(1.8f), glm::vec3(7.5f, 0.f, 0.f), 1.1f + static_cast<float>(lane_index) * 0.15f, collision_layer::character);
        create_dynamic_debug_box(right_name, glm::vec3(11.5f, 2.2f, z), glm::vec3(1.8f), glm::vec3(-6.5f, 0.f, 0.f), 1.1f + static_cast<float>(lane_index) * 0.15f, collision_layer::environment);
    }

    for (int stack_index = 0; stack_index < 3; ++stack_index) {
        const std::string name = "collision_side_stack_box_" + std::to_string(stack_index);
        create_dynamic_debug_box(name, glm::vec3(6.f, 2.2f + static_cast<float>(stack_index) * 2.05f, 8.f), glm::vec3(1.9f), glm::vec3(0.f), 1.0f, collision_layer::default_layer);
    }

    create_dynamic_debug_box("collision_side_stack_drop", glm::vec3(6.f, 11.5f, 8.f), glm::vec3(1.9f), glm::vec3(-0.5f, -2.5f, 0.f), 1.3f, collision_layer::default_layer);

    for (int rain_x = 0; rain_x < 3; ++rain_x) {
        for (int rain_z = 0; rain_z < 3; ++rain_z) {
            const std::string name = "collision_rain_box_" + std::to_string(rain_x) + "_" + std::to_string(rain_z);
            const glm::vec3 position(
                -4.f + static_cast<float>(rain_x) * 4.f,
                16.f + static_cast<float>((rain_x + rain_z) % 3) * 2.5f,
                -4.f + static_cast<float>(rain_z) * 4.f);
            const glm::vec3 velocity(
                (static_cast<float>(rain_x) - 1.f) * 0.8f,
                -3.0f - static_cast<float>((rain_x + rain_z) % 2) * 0.6f,
                (1.f - static_cast<float>(rain_z)) * 0.8f);
            create_dynamic_debug_box(name, position, glm::vec3(1.6f), velocity, 0.9f, collision_layer::default_layer);
        }
    }
}

