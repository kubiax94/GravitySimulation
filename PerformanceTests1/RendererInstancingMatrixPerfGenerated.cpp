#include "pch.h"
#include "CppUnitTest.h"
#include <memory>
#include <vector>
#include <string>
#include "..\GravitySimulation\scene_node.h"
#include "../GravitySimulation/Renderer.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PerformanceTests1
{
    TEST_CLASS(RendererInstancingMatrixPerfGenerated)
    {
        static constexpr int RendererCount = 10000;
        static constexpr int IterationCount = 32;

    public:
        TEST_METHOD(RendererGetVisualModelMatrixWithoutTranslationPerf)
        {
            std::vector<std::unique_ptr<scene_node>> nodes;
            std::vector<std::unique_ptr<renderer>> renderers;
            nodes.reserve(RendererCount);
            renderers.reserve(RendererCount);

            for (int i = 0; i < RendererCount; ++i)
            {
                auto node = std::make_unique<scene_node>("perf_node_" + std::to_string(i), static_cast<scene_node*>(nullptr), static_cast<i_scene_manager*>(nullptr));
                node->set_position(static_cast<float>(i), static_cast<float>(i % 11), static_cast<float>(i % 23));
                node->set_rotation(static_cast<float>((i * 5) % 360), static_cast<float>((i * 7) % 360), static_cast<float>((i * 11) % 360));
                node->set_scale(glm::vec3(1.0f + static_cast<float>(i % 4) * 0.25f));

                auto render = std::make_unique<renderer>(node.get(), nullptr, nullptr);
                render->set_visual_scale(glm::vec3(1.25f, 1.25f, 1.25f));

                renderers.push_back(std::move(render));
                nodes.push_back(std::move(node));
            }

            volatile float sink = 0.0f;
            for (int iteration = 0; iteration < IterationCount; ++iteration)
            {
                for (int i = 0; i < RendererCount; ++i)
                {
                    nodes[i]->set_position(static_cast<float>(iteration + i), static_cast<float>((iteration + i) % 19), static_cast<float>((iteration * 4 + i) % 37));
                    const glm::mat4 model = renderers[i]->get_visual_model_matrix_without_translation();
                    sink += model[0][0] + model[1][1] + model[2][2];
                }
            }

            Assert::IsTrue(sink != 0.0f || RendererCount == 0);
        }
    };
}
