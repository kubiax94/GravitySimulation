#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class frame_profiler
{
    using clock = std::chrono::high_resolution_clock;
    struct section_stat {
        double total_ms = 0.0;
        int count = 0;
        double current_frame_ms = 0.0;
        double max_frame_ms = 0.0;
    };

    std::unordered_map<std::string, section_stat> stats_;
    int frame_count_ = 0;
    int report_interval_ = 120;
    bool reporting_enabled_ = true;

    static inline thread_local frame_profiler* active_profiler_ = nullptr;

public:
    class scope_timer {
        frame_profiler* profiler_ = nullptr;
        std::string name_;
        clock::time_point start_{};
        bool active_ = false;

        void stop() {
            if (!active_ || !profiler_)
                return;

            const auto end = clock::now();
            const double ms = std::chrono::duration<double, std::milli>(end - start_).count();
            auto& stat = profiler_->stats_[name_];
            stat.total_ms += ms;
            stat.count += 1;
            stat.current_frame_ms += ms;
            active_ = false;
        }

    public:
        scope_timer() = default;

        scope_timer(frame_profiler& profiler, std::string name)
            : profiler_(&profiler), name_(std::move(name)), start_(clock::now()), active_(true) {
        }

        scope_timer(const scope_timer&) = delete;
        scope_timer& operator=(const scope_timer&) = delete;

        scope_timer(scope_timer&& other) noexcept
            : profiler_(std::exchange(other.profiler_, nullptr)),
              name_(std::move(other.name_)),
              start_(other.start_),
              active_(std::exchange(other.active_, false)) {
        }

        scope_timer& operator=(scope_timer&& other) noexcept {
            if (this != &other) {
                stop();
                profiler_ = std::exchange(other.profiler_, nullptr);
                name_ = std::move(other.name_);
                start_ = other.start_;
                active_ = std::exchange(other.active_, false);
            }

            return *this;
        }

        ~scope_timer() {
            stop();
        }
    };

    explicit frame_profiler(int report_interval = 120) : report_interval_(report_interval) {}

    [[nodiscard]] scope_timer measure(const std::string& name) {
        return scope_timer(*this, name);
    }

    static void set_active(frame_profiler* profiler) {
        active_profiler_ = profiler;
    }

    [[nodiscard]] static scope_timer measure_active(std::string name) {
        if (!active_profiler_)
            return {};

        return scope_timer(*active_profiler_, std::move(name));
    }

    void reset() {
        frame_count_ = 0;
        stats_.clear();
    }

    void set_reporting_enabled(bool enabled) {
        reporting_enabled_ = enabled;
    }

    void end_frame() {
        for (auto& [name, stat] : stats_)
            stat.max_frame_ms = std::max(stat.max_frame_ms, stat.current_frame_ms);

        ++frame_count_;
        if (frame_count_ < report_interval_) {
            for (auto& [name, stat] : stats_)
                stat.current_frame_ms = 0.0;
            return;
        }

        if (!reporting_enabled_) {
            frame_count_ = 0;
            stats_.clear();
            return;
        }

        struct report_entry {
            std::string name;
            double avg_frame_ms = 0.0;
            double avg_call_ms = 0.0;
            double max_frame_ms = 0.0;
        };

        std::vector<report_entry> report_entries;
        report_entries.reserve(stats_.size());
        for (const auto& [name, stat] : stats_) {
            report_entries.push_back({
                name,
                frame_count_ > 0 ? stat.total_ms / static_cast<double>(frame_count_) : 0.0,
                stat.count > 0 ? stat.total_ms / static_cast<double>(stat.count) : 0.0,
                stat.max_frame_ms
            });
        }

        std::sort(report_entries.begin(), report_entries.end(), [](const report_entry& lhs, const report_entry& rhs) {
            return lhs.avg_frame_ms > rhs.avg_frame_ms;
        });

        std::cout << "[frame_profiler] average over " << frame_count_ << " frames:\n";
        for (const auto& entry : report_entries)
            std::cout << "  " << entry.name << ": " << entry.avg_frame_ms << " ms/frame"
                      << " (avg call " << entry.avg_call_ms << " ms, max frame " << entry.max_frame_ms << " ms)\n";

        frame_count_ = 0;
        stats_.clear();
    }
};