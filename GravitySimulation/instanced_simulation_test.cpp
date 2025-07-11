#include "instanced_simulation_test.h"
#include "instance_buffer_manager.h"
#include "instanced_renderer.h"
#include "Mesh.h"
#include "Shape.h"
#include "Renderer.h"
#include "Shader.h"
#include "unit_system.h"
#include "physics_data.h"
#include "rigid_body.h"
#include <random>
#include <glm/gtc/matrix_transform.hpp>

namespace simtest_instanced {

    struct planet_data
    {
        std::string name;
        float mass = 0.f;
        float diameter = 0.f;
        float distance_to_sun = 0.f;
        glm::vec3 color = glm::vec3(1.0f, 0.5f, 0.31f);
    };

    std::vector<planet_data> planetary_data = {
        {"Mercury", 0.330e24f, 4879, 57.9e6f, glm::vec3(0.8f, 0.7f, 0.6f)},
        {"Venus", 4.87e24f, 12104, 108.2e6f, glm::vec3(1.0f, 0.8f, 0.4f)},
        {"Earth", 5.97e24f, 12756, 149.6e6f, glm::vec3(0.3f, 0.7f, 1.0f)},
        {"Mars", 0.642e24f, 6792, 227.9e6f, glm::vec3(0.9f, 0.4f, 0.2f)},
        {"Jupiter", 1898e24f, 142984, 778.6e6f, glm::vec3(0.9f, 0.7f, 0.5f)},
        {"Saturn", 568e24f, 120536, 1433.5e6f, glm::vec3(0.9f, 0.8f, 0.6f)},
        {"Uranus", 86.8e24f, 51118, 2872.5e6f, glm::vec3(0.4f, 0.8f, 0.9f)},
        {"Neptune", 102e24f, 49528, 4495.1e6f, glm::vec3(0.3f, 0.5f, 0.9f)}
    };

    void init_instanced_gravity_test(scene* s_to_init, 
                                   InstancedRenderer** out_planet_renderer,
                                   InstanceBufferManager** out_instance_manager,
                                   renderer** out_sun_renderer)
    {
        unit_system u_sys(1e24f, 1e6f, 3.872e6f / 3600.f);

        // Create sun (not instanced)
        auto* sun_node = s_to_init->create_scene_node("Sun");
        shader* sun_shader = new shader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/lightsource.vs.shader", 
                                       "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/sun.fs.shader");

        auto sphere_mesh_data = Shape::GenerateSphere();
        auto* sphere_mesh = new Mesh(sphere_mesh_data);
        
        auto* sun_render = sun_node->add_component<renderer>(sun_node, sun_shader, sphere_mesh);
        *out_sun_renderer = sun_render;

        float sun_mass = u_sys.mass(1.9885e30f);
        float dia_scale = 12756.f;

        auto* sun_physics_data = new physics_data(
            glm::vec4(0, 0, 0, sun_mass),
            glm::vec4(0, 0, 0, sun_mass),
            { 0, 0, 0, sun_mass });

        sun_node->add_component<rigid_body>(sun_node, sun_physics_data);
        sun_node->set_global_scale(glm::vec3(1391000/dia_scale));

        // Create instanced rendering system for planets
        auto* instance_manager = new InstanceBufferManager();
        *out_instance_manager = instance_manager;

        // Create instanced shader for planets
        shader* planet_shader = new shader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/instanced.vs.shader", 
                                         "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/instanced.fs.shader");

        auto* instanced_renderer = new InstancedRenderer(planet_shader, sphere_mesh, instance_manager);
        *out_planet_renderer = instanced_renderer;

        // Create a shader group for planets
        size_t planetShaderGroup = instance_manager->createShaderGroup("planets");

        // Add planets as instances
        for (const auto& planet : planetary_data)
        {
            float v = sqrt(u_sys.scaled_G() * sun_mass / u_sys.distance(planet.distance_to_sun));
            
            // Create transform matrix for this planet
            glm::vec3 position(u_sys.distance(planet.distance_to_sun) + 109.f, 0, 0);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
            
            // Create instance data
            InstanceData planetInstance;
            planetInstance.modelMatrix = transform;
            planetInstance.color = planet.color;
            planetInstance.scale = planet.diameter / dia_scale;

            // Add to instance manager
            size_t instanceIndex = instance_manager->addInstance(planetInstance);
            instance_manager->addInstanceToShaderGroup(planetShaderGroup, instanceIndex);

            // Create physics data for scene node (still needed for physics)
            auto p_physics_data = physics_data(
                glm::vec4(position, u_sys.mass(planet.mass)),
                glm::vec4(0, 0, v, u_sys.mass(planet.mass)),
                { 0, 0, 0, u_sys.mass(planet.mass) });

            auto* planet_node = s_to_init->create_scene_node(planet.name);
            planet_node->add_component<rigid_body>(planet_node, p_physics_data);
            planet_node->set_global_position(p_physics_data.position);
            planet_node->set_global_scale(glm::vec3(planet.diameter/dia_scale));
        }

        // Update GPU buffers
        instance_manager->updateGPUBuffers();
    }
}