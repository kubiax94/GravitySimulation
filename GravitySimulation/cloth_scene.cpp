#include "cloth_scene.h"

#include <cmath>
#include <glm/gtx/quaternion.hpp>

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

glm::quat align_forward_to(const glm::vec3& direction) {
    const glm::vec3 normalized_direction = glm::normalize(direction);
    const float alignment = glm::dot(transform::Forward, normalized_direction);

    if (alignment > 0.9999f)
        return glm::quat(1.f, 0.f, 0.f, 0.f);

    if (alignment < -0.9999f)
        return glm::angleAxis(glm::radians(180.0f), glm::vec3(0.f, 1.f, 0.f));

    return glm::rotation(transform::Forward, normalized_direction);
}

void update_spring_visual(scene_node* spring_node, const glm::vec3& start, const glm::vec3& end) {
    if (!spring_node)
        return;

    const glm::vec3 delta = end - start;
    const float length = glm::length(delta);

    transform spring_transform;
    spring_transform.setPosition(start);

    if (length > 0.0001f)
        spring_transform.set_rotation_quat(align_forward_to(delta));

    spring_transform.setScale(glm::vec3(1.f, 1.f, length));
    spring_node->set_transform(spring_transform);
}

class spring_link_component final : public component
{
    scene_node* start_node_ = nullptr;
    scene_node* end_node_ = nullptr;

public:
    spring_link_component(scene_node* owner, scene_node* start_node, scene_node* end_node)
        : component(owner), start_node_(start_node), end_node_(end_node) {
    }

    type_id_t get_type_id() const override {
        return ::get_type_id<spring_link_component>();
    }

    void update() override {
        if (!owner_node_ || !start_node_ || !end_node_)
            return;

        update_spring_visual(owner_node_, start_node_->get_global_position(), end_node_->get_global_position());
    }
};

}

cloth_scene::cloth_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void cloth_scene::initialize_scene_content() {
    auto* cam_node = create_scene_node("cam");
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(glm::vec3(0.f, 90.f, 260.f));
    cam_node->set_global_rotation(glm::vec3(0.f, 0.f, 0.f));

    auto* grid_node = create_scene_node("grid");
    static MeshData grid_data = g_shape::generate_grid_lines(64, 50.f);
    grid_mesh_ = std::make_unique<Mesh>(grid_data);
    grid_mesh_->type = MeshType::LINES;
    grid_shader_ = std::make_unique<shader>("GravitySimulation/default.vs.shader", "GravitySimulation/default.fs.shader");
    grid_node->add_component<renderer>(grid_node, grid_shader_.get(), grid_mesh_.get());
    grid_node->set_global_position(glm::vec3(0.f, .001f, 0.f));

    static MeshData cloth_particle_data = g_shape::generate_sphere(1.f, 24, 16);
    static MeshData cloth_link_data = create_spring_segment_mesh();

    cloth_particle_shader_ = std::make_unique<shader>("GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");
    cloth_compute_shader_ = std::make_unique<compute_shader>("GravitySimulation/cloth_simulation.glsl");
    cloth_particle_mesh_ = std::make_unique<Mesh>(cloth_particle_data);
    cloth_link_mesh_ = std::make_unique<Mesh>(cloth_link_data);
    cloth_link_mesh_->type = MeshType::LINES;

    register_compute_shader(cloth_compute_shader_.get());

    constexpr int columns = 13;
    constexpr int rows = 9;
    constexpr float spacing = 12.f;
    constexpr float top_y = 150.f;
    constexpr float mass_radius = 1.6f;
    constexpr float particle_mass = 1.f;
    constexpr float structural_stiffness = 14.f;
    constexpr float shear_stiffness = 9.f;
    constexpr float bend_stiffness = 2.5f;

    auto particle_index = [columns](int x, int y) {
        return static_cast<size_t>(y * columns + x);
    };

    std::vector<scene_node*> particle_nodes(columns * rows, nullptr);
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
            particle_node->add_component<renderer>(particle_node, cloth_particle_shader_.get(), cloth_particle_mesh_.get());

            auto* particle_data = new physics_data{
                glm::vec4(position, pinned ? 0.f : particle_mass),
                glm::vec4(pinned ? glm::vec3(0.f) : initial_velocity, pinned ? 0.f : particle_mass),
                glm::vec4(0.f, 0.f, 0.f, pinned ? 0.f : particle_mass)};

            auto* body = new rigid_body(particle_node, particle_data);
            body->set_compute_shader(cloth_compute_shader_->get_id());
            particle_node->add_component(body);

            particle_nodes[index] = particle_node;
            particle_positions[index] = position;
        }
    }

    auto add_link = [&](size_t a, size_t b, float stiffness) {
        auto* spring_node = create_scene_node("cloth_link_" + std::to_string(constraints.size()));
        spring_node->add_component<renderer>(spring_node, grid_shader_.get(), cloth_link_mesh_.get());
        spring_node->add_component(new spring_link_component(spring_node, particle_nodes[a], particle_nodes[b]));

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
