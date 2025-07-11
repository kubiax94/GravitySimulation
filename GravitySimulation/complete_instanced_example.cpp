// Complete example showing instanced rendering with physics integration
// This demonstrates the full power of compute groups and shader groups

#define GLM_ENABLE_EXPERIMENTAL
#include <iostream>
#include <vector>

#include "Shader.h"
#include "Shape.h"
#include "Camera.h"
#include "Time.h"
#include "Renderer.h"
#include "Scene.h"
#include "unit_system.h"
#include "uuid.h"
#include "input_system.h"

// Instanced rendering system
#include "instanced_simulation_test.h"
#include "instance_buffer_manager.h"
#include "instanced_renderer.h"
#include "instanced_physics_system.h"

#include <GLFW/glfw3.h>
#include <glm/gtx/string_cast.hpp>

void sim_key_callback(GLFWwindow* window, int key, int scancode, int action, int modes) {
    if (action == GLFW_PRESS) {
        input_system::on_key_press(key);
    } else if (action == GLFW_RELEASE) {
        input_system::on_key_release(key);
    }
}

void sim_mouse_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    input_system::on_mouse_move(xpos, ypos);
}

void sim_mouse_buttons_callback(GLFWwindow* window, int button, int action, int modes) {
    if (action == GLFW_PRESS) {
        input_system::on_mouse_button_press(button);
    } else if (action == GLFW_RELEASE) {
        input_system::on_mouse_button_release(button);
    }
}

int main_complete_instanced_example()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Complete Instanced Gravity Simulation", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Setup callbacks
    glfwSetKeyCallback(window, sim_key_callback);
    glfwSetCursorPosCallback(window, sim_mouse_pos_callback);
    glfwSetMouseButtonCallback(window, sim_mouse_buttons_callback);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Create grid for reference
    auto gridVert = Shape::GenerateGridLines(64, 50.f);
    Mesh gridMesh(gridVert);
    gridMesh.type = MeshType::LINES;
    shader gridShader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/default.vs.shader", 
                     "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/default.fs.shader");

    sim::time* time1 = new sim::time();

    scene cscene(time1);
    scene_node* cam_node = cscene.create_scene_node("cam");
    scene_node* grid_node = cscene.create_scene_node("grid");

    // Setup camera
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(glm::vec3(0.f, 280.f, 400.f));
    cam_node->set_global_rotation(glm::vec3(-45.f, 0.f, 0.f));

    // Setup grid
    renderer grid_renderer(grid_node, &gridShader, &gridMesh);
    grid_renderer.attach_to(grid_node);
    grid_node->set_global_position(glm::vec3(.0f, .001f, .0f));

    auto* cam = cam_node->find_component<Camera>();

    // Initialize complete instanced system with physics
    InstancedRenderer* planetRenderer = nullptr;
    InstanceBufferManager* instanceManager = nullptr;
    InstancedPhysicsSystem* physicsSystem = nullptr;
    renderer* sunRenderer = nullptr;

    simtest_instanced::init_instanced_gravity_test(&cscene, &planetRenderer, &instanceManager, &physicsSystem, &sunRenderer);

    std::cout << "=== Instanced Rendering System Initialized ===" << std::endl;
    std::cout << "Planet instances: " << instanceManager->getInstanceCount() << std::endl;
    std::cout << "Shader groups: " << instanceManager->getShaderGroupCount() << std::endl;
    std::cout << "Compute groups: " << instanceManager->getComputeGroupCount() << std::endl;
    std::cout << "Physics objects: " << physicsSystem->getPhysicsData().size() << std::endl;
    std::cout << "===============================================" << std::endl;

    // Performance tracking
    int frameCount = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        time1->update_time(static_cast<float>(glfwGetTime()));
        cam->process_input(time1->delta_time);

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Physics update (using compute groups for batch operations)
        while (time1->should_fixed_update()) {
            // Update traditional scene objects (sun, etc.)
            cscene.update();
            
            // Update instanced physics system
            physicsSystem->updatePhysics(time1->get_fixed_delta_time());
            
            // Sync instance positions with physics
            physicsSystem->syncInstancesToPhysics();
            
            time1->reduce_accumulator();
        }

        // Render sun (traditional rendering for special objects)
        sunRenderer->draw(cam, [&](shader& s) {
            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
            s.set_uni_vec3("viewPos", cam->get_transform()->get_global_position());
            s.set_uni_float("intensity", 0.75f);
        });

        // Render all planets with single instanced draw call
        // This replaces N individual draw calls with 1 instanced call
        planetRenderer->drawAllInstances(cam, [&](shader& s) {
            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
            s.set_uni_vec3("viewPos", cam->get_transform()->get_global_position());
            s.set_uni_vec3("lightPos", sunRenderer->get_transform()->get_global_position());
            s.set_uni_float("useInstanceColor", 1.0f); // Use per-instance colors
        });

        // Optionally render specific shader groups:
        // planetRenderer->drawInstanced(cam, shaderGroupIndex, callback);

        // Render grid
        grid_renderer.draw(cam);

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Performance logging
        frameCount++;
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= 1.0) {
            std::cout << "FPS: " << frameCount << " | Instances: " << instanceManager->getInstanceCount() << std::endl;
            frameCount = 0;
            lastTime = currentTime;
        }
    }

    // Cleanup
    delete planetRenderer;
    delete physicsSystem;
    delete instanceManager;
    delete time1;

    glfwTerminate();
    return 0;
}

// This function demonstrates the key benefits:
void demonstrate_instancing_benefits() {
    std::cout << "\n=== INSTANCING BENEFITS DEMONSTRATION ===" << std::endl;
    std::cout << "BEFORE (Individual Rendering):" << std::endl;
    std::cout << "  - 8 planets = 8 separate draw calls" << std::endl;
    std::cout << "  - Each call: setup shader, set uniforms, draw" << std::endl;
    std::cout << "  - GPU state changes for each object" << std::endl;
    std::cout << "  - CPU overhead for each draw call" << std::endl;
    std::cout << std::endl;
    std::cout << "AFTER (Instanced Rendering):" << std::endl;
    std::cout << "  - 8 planets = 1 instanced draw call" << std::endl;
    std::cout << "  - Setup once, draw all instances" << std::endl;
    std::cout << "  - Minimal GPU state changes" << std::endl;
    std::cout << "  - Dramatic CPU overhead reduction" << std::endl;
    std::cout << std::endl;
    std::cout << "COMPUTE GROUPS:" << std::endl;
    std::cout << "  - Batch physics calculations" << std::endl;
    std::cout << "  - Organize similar compute operations" << std::endl;
    std::cout << "  - Efficient GPU compute utilization" << std::endl;
    std::cout << std::endl;
    std::cout << "SHADER GROUPS:" << std::endl;
    std::cout << "  - Organize objects by material/shader" << std::endl;
    std::cout << "  - Render different types efficiently" << std::endl;
    std::cout << "  - Flexible rendering pipeline" << std::endl;
    std::cout << "===============================================\n" << std::endl;
}