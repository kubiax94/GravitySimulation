#include "pch.h"
#include "CppUnitTest.h"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <vector>

#include "..\GravitySimulation\Mesh.h"
#include "..\GravitySimulation\Renderer.h"
#include "../GravitySimulation/scene_node.h"
#include "..\GravitySimulation\aabb_collider.h"
#include "..\GravitySimulation\collider.h"
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
            window = glfwCreateWindow(64, 64, "SceneNodeComponentLookupPerfTest", nullptr, nullptr);
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

    void sync_renderer_aabb_collider_hot_path(renderer& render) {
        auto* node = render.get_node();
        if (!node)
            return;

        auto* aabb = node->find_component<aabb_collider>();
        if (aabb && !aabb->is_auto_generated())
            return;

        if (!aabb) {
            auto* existing_collider = node->find_component<collider>();
            if (existing_collider && !existing_collider->is_auto_generated())
                return;
        }

        const uint64_t renderer_revision = render.get_instance_revision(true);
        const bounding_box local_bounds = render.get_local_bounding_box();
        if (!local_bounds.valid)
            return;

        if (!aabb) {
            aabb = node->add_component<aabb_collider>(node, local_bounds);
            if (!aabb)
                return;
            aabb->set_auto_generated(true);
        }

        if (aabb->get_source_revision() == renderer_revision)
            return;

        aabb->set_auto_generated(true);
        aabb->set_local_bounds(local_bounds);
        aabb->set_source_revision(renderer_revision);
    }

    class test_collider final : public collider {
    public:
        explicit test_collider(scene_node* node)
            : collider(node) {
        }

        static type_id_t type_id() {
            return ::get_type_id<test_collider>();
        }

        type_id_t get_type_id() const override {
            return type_id();
        }
    };
}

namespace PerformanceTests1 {
    TEST_CLASS(SceneNodeComponentLookupPerfTest) {
    public:
        TEST_METHOD(SceneNodeComponentLookupHotPath) {
            constexpr int NodeCount = 8000;
            constexpr int LookupPasses = 256;

            gl_test_context context;

            MeshData mesh_data;
            mesh_data.vertecies = {
                {{-0.5f, -0.5f, -0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
                {{0.5f, -0.5f, -0.5f}, {0.f, 0.f, 1.f}, {1.f, 0.f}},
                {{0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f}},
                {{-0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
                {{-0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
                {{0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 0.f}},
                {{0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f}},
                {{-0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 1.f}}
            };
            mesh_data.indices = {
                0u, 1u, 2u, 0u, 2u, 3u,
                4u, 5u, 6u, 4u, 6u, 7u,
                0u, 1u, 5u, 0u, 5u, 4u,
                2u, 3u, 7u, 2u, 7u, 6u,
                1u, 2u, 6u, 1u, 6u, 5u,
                0u, 3u, 7u, 0u, 7u, 4u
            };
            Mesh mesh(mesh_data);

            scene_node root("root", static_cast<scene_node*>(nullptr), static_cast<i_scene_manager*>(nullptr));
            std::vector<std::unique_ptr<scene_node>> nodes;
            std::vector<std::unique_ptr<renderer>> renderers;
            nodes.reserve(NodeCount);
            renderers.reserve(NodeCount);

            for (int i = 0; i < NodeCount; ++i) {
                auto node = std::make_unique<scene_node>("node_" + std::to_string(i), &root, static_cast<i_scene_manager*>(nullptr));
                node->add_component<test_collider>(node.get());
                node->set_position(glm::vec3(static_cast<float>(i % 128), static_cast<float>((i / 128) % 64), static_cast<float>(i % 17)));
                node->set_rotation(glm::vec3(static_cast<float>(i % 360), static_cast<float>((i * 3) % 360), static_cast<float>((i * 7) % 360)));
                auto render = std::make_unique<renderer>(node.get(), nullptr, &mesh);
                render->set_visual_scale(glm::vec3(1.0f + static_cast<float>(i % 5) * 0.05f));
                root.add_child(node.get());
                renderers.push_back(std::move(render));
                nodes.push_back(std::move(node));
            }

            volatile size_t sink = 0;
            for (int pass = 0; pass < LookupPasses; ++pass) {
                for (size_t i = 0; i < nodes.size(); ++i) {
                    auto& node = nodes[i];
                    auto& render = renderers[i];
                    node->set_rotation(glm::vec3(static_cast<float>((pass + static_cast<int>(i)) % 360), static_cast<float>((pass * 2 + static_cast<int>(i)) % 360), static_cast<float>((pass * 5 + static_cast<int>(i)) % 360)));
                    sync_renderer_aabb_collider_hot_path(*render);
                    auto* collider_component = node->find_component<collider>();
                    auto* aabb_component = node->find_component<aabb_collider>();
                    const bool has_aabb = node->has_component<aabb_collider>();
                    sink += collider_component != nullptr ? 1u : 0u;
                    sink += aabb_component != nullptr ? 1u : 0u;
                    sink += has_aabb ? 1u : 0u;
                }
            }

            Assert::IsTrue(sink > 0u, L"lookup sink should be non-zero");
        }
    };
}
