#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "resource.h"

class scene_loader
{
    struct resource_entry
    {
        resource* resource_ptr = nullptr;
        bool started = false;
        bool finalized = false;
        bool failed = false;
        std::function<void(resource&)> on_finalized;
    };

    std::vector<resource_entry> resources_;
    bool started_ = false;
    bool completed_ = false;
    bool failed_ = false;

    resource_entry* find_entry(resource* resource_ptr);
    const resource_entry* find_entry(resource* resource_ptr) const;

public:
    scene_loader() = default;

    void reset();
    void add_resource(resource& resource_ref);
    void add_resource(resource& resource_ref, std::function<void(resource&)> on_finalized);
    void start(async_priority priority = async_priority::MEDIUM);
    void update();
    void cancel();

    bool is_started() const { return started_; }
    bool is_completed() const { return completed_; }
    bool is_failed() const { return failed_; }
    bool is_ready() const { return started_ && !failed_ && completed_; }
    float get_progress() const;
    std::size_t get_total_resources() const { return resources_.size(); }
    std::size_t get_completed_resources() const;
};
