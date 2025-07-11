// Example: How to use the new instanced rendering system
// This demonstrates the integration of instanced rendering with compute and shader groups

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

// New instanced rendering includes
#include "instanced_simulation_test.h"
#include "instance_buffer_manager.h"
#include "instanced_renderer.h"

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

int main_instanced_example()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Instanced Gravity Simulation", NULL, NULL);
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

    // Initialize instanced rendering system
    InstancedRenderer* planetRenderer = nullptr;
    InstanceBufferManager* instanceManager = nullptr;
    renderer* sunRenderer = nullptr;

    simtest_instanced::init_instanced_gravity_test(&cscene, &planetRenderer, &instanceManager, &sunRenderer);

    std::cout << "Instanced rendering initialized with " << instanceManager->getInstanceCount() << " planet instances" << std::endl;
    std::cout << "Shader groups: " << instanceManager->getShaderGroupCount() << std::endl;
    std::cout << "Compute groups: " << instanceManager->getComputeGroupCount() << std::endl;

    while (!glfwWindowShouldClose(window)) {
        time1->update_time(static_cast<float>(glfwGetTime()));
        cam->process_input(time1->delta_time);

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        while (time1->should_fixed_update()) {
            cscene.update();
            time1->reduce_accumulator();
        }

        // Render sun (traditional rendering)
        sunRenderer->draw(cam, [&](shader& s) {
            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
            s.set_uni_vec3("viewPos", cam->get_transform()->get_global_position());
            s.set_uni_float("intensity", 0.75f);
        });

        // Render all planets with a single instanced draw call
        planetRenderer->drawAllInstances(cam, [&](shader& s) {
            s.set_uni_vec3("lightColor", glm::vec3(1.0f, .8f, .3f));
            s.set_uni_vec3("viewPos", cam->get_transform()->get_global_position());
            s.set_uni_vec3("lightPos", sunRenderer->get_transform()->get_global_position());
            s.set_uni_float("useInstanceColor", 1.0f); // Use per-instance colors
        });

        // Render grid
        grid_renderer.draw(cam);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    delete planetRenderer;
    delete instanceManager;
    delete time1;

    glfwTerminate();
    return 0;
}