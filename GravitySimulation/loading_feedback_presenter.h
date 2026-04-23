#pragma once

#include <string>

class engine;
class scene;
class scene_loader;

class loading_feedback_presenter
{
public:
    virtual ~loading_feedback_presenter() = default;

    virtual void on_loading_begin(engine& engine, const scene& active_scene, const scene_loader& loader);
    virtual void on_loading_update(engine& engine, const scene& active_scene, const scene_loader& loader) = 0;
    virtual void on_loading_complete(engine& engine, const scene& active_scene, const scene_loader& loader);
    virtual void on_loading_failed(engine& engine, const scene& active_scene, const scene_loader& loader);
    virtual void render(engine& engine, const scene& active_scene, const scene_loader& loader);
};

class window_title_loading_feedback final : public loading_feedback_presenter
{
    std::string base_title_;
    bool active_ = false;

public:
    explicit window_title_loading_feedback(std::string base_title = "");

    void on_loading_begin(engine& engine, const scene& active_scene, const scene_loader& loader) override;
    void on_loading_update(engine& engine, const scene& active_scene, const scene_loader& loader) override;
    void on_loading_complete(engine& engine, const scene& active_scene, const scene_loader& loader) override;
    void on_loading_failed(engine& engine, const scene& active_scene, const scene_loader& loader) override;
};
