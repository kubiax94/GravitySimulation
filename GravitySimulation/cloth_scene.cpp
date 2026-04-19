#include "cloth_scene.h"

#include <cmath>

#include "compute_shader.h"
#include "g_shape.h"
#include "Renderer.h"
#include "rigid_body.h"

namespace {

struct alignas(16) cloth_constraint_data
{
    glm::uvec2 particle_indices = glm::uvec2(0u);
    float rest_length = 0.f;
    float stiffness = 0.f;
};

MeshData create_spring_segment_mesh() {
    MeshData mesh_data;

    Vertex start{};
    start.Position = glm::vec3(0.f);
    start.Normal = glm::vec3(0.f, 1.f, 0.f);
    mesh_data.vertecies.push_back(start);

    Vertex end{};
    end.Position = glm::vec3(0.f, 0.f, -1.f);
    end.Normal = glm::vec3(0.f, 1.f, 0.f);
    mesh_data.vertecies.push_back(end);

    mesh_data.indices = { 0, 1 };
    return mesh_data;
}

}

cloth_scene::cloth_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void cloth_scene::initialize_scene_content() {
  auto& assets = get_asset_manager();

    auto* cam_node = create_scene_node("cam");
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(glm::vec3(0.f, 90.f, 260.f));
    cam_node->set_global_rotation(glm::vec3(0.f, 0.f, 0.f));

    auto* grid_node = create_scene_node("grid");
    static MeshData grid_data = g_shape::generate_grid_lines(64, 50.f);
    grid_mesh_ = assets.create_mesh(grid_data);
    grid_mesh_->type = MeshType::LINES;
    grid_shader_ = assets.create_shader("cloth.grid", "GravitySimulation/default.vs.shader", "GravitySimulation/default.fs.shader");
    grid_node->add_component<renderer>(grid_node, grid_shader_, grid_mesh_);
    grid_node->set_global_position(glm::vec3(0.f, .001f, 0.f));

    static MeshData cloth_particle_data = g_shape::generate_sphere(1.f, 24, 16);
    static MeshData cloth_link_data = create_spring_segment_mesh();

    cloth_particle_shader_ = assets.create_shader("cloth.particle", "GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");
    cloth_link_shader_ = assets.create_shader("cloth.link", "GravitySimulation/cloth_link.vs.shader", "GravitySimulation/default.fs.shader");
    cloth_compute_shader_ = assets.create_compute_shader("cloth.compute", "GravitySimulation/cloth_simulation.glsl");
    cloth_particle_mesh_ = assets.create_mesh(cloth_particle_data);
    cloth_link_mesh_ = assets.create_mesh(cloth_link_data);
    cloth_link_mesh_->type = MeshType::LINES;

    register_compute_shader(cloth_compute_shader_);

    constexpr int columns = 13;
    constexpr int rows = 9;
    constexpr float spacing = 12.f;
    constexpr float top_y = 150.f;
    constexpr float mass_radius = 1.6f;
    constexpr float particle_mass = 1.f;
    constexpr float structural_stiffness = 44.f;
    constexpr float shear_stiffness = 6.f;
    constexpr float bend_stiffness = 1.2f;

    auto particle_index = [columns](int x, int y) {
        return static_cast<size_t>(y * columns + x);
    };

    std::vector<glm::vec3> particle_positions(columns * rows, glm::vec3(0.f));
    std::vector<cloth_constraint_data> constraints;
    constraints.reserve((columns - 1) * rows + (rows - 1) * columns + 2 * (columns - 1) * (rows - 1));

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const size_t index = particle_index(x, y);
            const glm::vec3 position(
                (static_cast<float>(x) - static_cast<float>(columns - 1) * 0.5f) * spacing,
                top_y - static_cast<float>(y) * spacing,
                std::sin(static_cast<float>(x) * 0.45f) * 1.2f + std::cos(static_cast<float>(y) * 0.55f) * 0.6f);
            const bool pinned = y == 0 && (x == 0 || x == columns - 1);
            const glm::vec3 initial_velocity(
                0.15f * static_cast<float>(x - columns / 2),
                0.f,
                std::sin(static_cast<float>(x + y) * 0.4f) * 2.5f);

            auto* particle_node = create_scene_node("cloth_particle_" + std::to_string(index));
            particle_node->set_global_position(position);
            particle_node->set_global_scale(glm::vec3(mass_radius));
            auto* particle_renderer = particle_node->add_component<renderer>(particle_node, cloth_particle_shader_, cloth_particle_mesh_);
            particle_renderer->set_gpu_driven_positions(true);
            particle_renderer->set_gpu_physics_index(static_cast<int>(index));

            auto* particle_data = new physics_data{
                glm::vec4(position, pinned ? 0.f : particle_mass),
                glm::vec4(pinned ? glm::vec3(0.f) : initial_velocity, pinned ? 0.f : particle_mass),
                glm::vec4(0.f, 0.f, 0.f, pinned ? 0.f : particle_mass)};

            auto* body = new rigid_body(particle_node, particle_data);
            body->set_compute_shader(cloth_compute_shader_->get_id());
            particle_node->add_component(body);

            particle_positions[index] = position;
        }
    }

    auto add_link = [&](size_t a, size_t b, float stiffness) {
        auto* spring_node = create_scene_node("cloth_link_" + std::to_string(constraints.size()));
        spring_node->set_global_scale(glm::vec3(static_cast<float>(a + 1), static_cast<float>(b + 1), 1.f));
        auto* spring_renderer = spring_node->add_component<renderer>(spring_node, cloth_link_shader_, cloth_link_mesh_);
        spring_renderer->set_visual_scale(glm::vec3(1.f));
        spring_renderer->set_gpu_driven_positions(true);

        constraints.push_back({
            glm::uvec2(static_cast<unsigned int>(a), static_cast<unsigned int>(b)),
            glm::distance(particle_positions[a], particle_positions[b]),
            stiffness
        });
    };

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const size_t current = particle_index(x, y);

            if (x + 1 < columns)
                add_link(current, particle_index(x + 1, y), structural_stiffness);

            if (y + 1 < rows)
                add_link(current, particle_index(x, y + 1), structural_stiffness);

            if (x + 1 < columns && y + 1 < rows) {
                add_link(current, particle_index(x + 1, y + 1), shear_stiffness);
                add_link(particle_index(x + 1, y), particle_index(x, y + 1), shear_stiffness);
            }

            if (x + 2 < columns)
                add_link(current, particle_index(x + 2, y), bend_stiffness);

            if (y + 2 < rows)
                add_link(current, particle_index(x, y + 2), bend_stiffness);
        }
    }

    cloth_compute_shader_->use();
    cloth_compute_shader_->add_ssbo(1, constraints);
    cloth_compute_shader_->set_uni_int("constraintCount", static_cast<int>(constraints.size()));
    cloth_compute_shader_->set_uni_float("gravityAcceleration", 36.f);
    cloth_compute_shader_->set_uni_float("springDamping", 1.15f);
    cloth_compute_shader_->set_uni_float("velocityDamping", 0.988f);
    cloth_compute_shader_->set_uni_float("floorHeight", 2.f);
    cloth_compute_shader_->set_uni_float("floorBounce", 0.05f);
    cloth_compute_shader_->set_uni_float("tangentialDamping", 0.85f);
    cloth_compute_shader_->set_uni_vec3("windDirection", glm::normalize(glm::vec3(0.35f, -0.1f, 1.0f)));
    cloth_compute_shader_->set_uni_float("windStrength", 12.f);
    cloth_compute_shader_->set_uni_float("windPulseStrength", 26.f);
    cloth_compute_shader_->set_uni_float("windPulseFrequency", 1.8f);
    cloth_compute_shader_->set_uni_float("windTurbulence", 18.f);
}
