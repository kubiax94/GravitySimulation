#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

class frame_profiler
{
    using clock = std::chrono::high_resolution_clock;
    struct section_stat {
        double total_ms = 0.0;
        int count = 0;
    };

    std::unordered_map<std::string, section_stat> stats_;
    int frame_count_ = 0;
    int report_interval_ = 120;

public:
    class scope_timer {
        frame_profiler& profiler_;
        std::string name_;
        clock::time_point start_;

    public:
        scope_timer(frame_profiler& profiler, std::string name)
            : profiler_(profiler), name_(std::move(name)), start_(clock::now()) {
        }

        ~scope_timer() {
            const auto end = clock::now();
            const double ms = std::chrono::duration<double, std::milli>(end - start_).count();
            auto& stat = profiler_.stats_[name_];
            stat.total_ms += ms;
            stat.count += 1;
        }
    };

    explicit frame_profiler(int report_interval = 120) : report_interval_(report_interval) {}

    [[nodiscard]] scope_timer measure(const std::string& name) {
        return scope_timer(*this, name);
    }

    void end_frame() {
        ++frame_count_;
        if (frame_count_ < report_interval_)
            return;

        std::cout << "[frame_profiler] average over " << frame_count_ << " frames:\n";
        for (const auto& [name, stat] : stats_) {
            const double avg = stat.count > 0 ? stat.total_ms / static_cast<double>(stat.count) : 0.0;
            std::cout << "  " << name << ": " << avg << " ms\n";
        }

        frame_count_ = 0;
        stats_.clear();
    }
};