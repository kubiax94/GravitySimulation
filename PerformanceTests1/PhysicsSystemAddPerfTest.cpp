#include "pch.h"
#include "CppUnitTest.h"
#include <filesystem>
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "..\GravitySimulation\Scene.h"
#include "..\GravitySimulation\Renderer.h"
#include "..\GravitySimulation\galactic_simulation_test.h"
#include "../GravitySimulation/physics_system.h"
#define GLFW_INCLUDE_NONE





using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace {
    struct gl_test_context {
        GLFWwindow* window = nullptr;
        bool initialized = false;

        gl_test_context() {
            initialized = glfwInit() == GLFW_TRUE;
            Assert::IsTrue(initialized, L"glfwInit failed");
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            window = glfwCreateWindow(64, 64, "PhysicsSystemAddPerfTest", nullptr, nullptr);
            Assert::IsNotNull(window, L"glfwCreateWindow failed");
            glfwMakeContextCurrent(window);
            Assert::IsTrue(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, L"gladLoadGLLoader failed");
            glViewport(0, 0, 64, 64);
        }

        ~gl_test_context() {
            if (window)
                glfwDestroyWindow(window);
            if (initialized)
                glfwTerminate();
        }
    };
}

namespace PerformanceTests1 {
    TEST_CLASS(PhysicsSystemAddPerfTest) {
    public:
        TEST_METHOD(StressSceneInitialization) {
            constexpr int BodyCount = 5000;

            gl_test_context context;
            scene test_scene;
            std::vector<renderer*> renderers;
            renderers.reserve(BodyCount);

            simtest::stress_test(&test_scene, renderers, BodyCount);

            volatile size_t sink = renderers.size();
            Assert::IsTrue(sink == BodyCount, L"unexpected renderer count");
        }
    };
}
