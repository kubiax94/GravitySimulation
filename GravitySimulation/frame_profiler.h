#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class frame_profiler
{
    using clock = std::chrono::high_resolution_clock;

public:
    struct report_entry {
        std::string name;
        double avg_frame_ms = 0.0;
        double avg_call_ms = 0.0;
        double max_frame_ms = 0.0;
    };

    struct value_report_entry {
        std::string name;
        double avg_frame_value = 0.0;
        double max_frame_value = 0.0;
    };

    struct report_snapshot {
        int frame_count = 0;
        std::vector<report_entry> sections;
        std::vector<value_report_entry> values;
    };

private:
    struct section_stat {
        double total_ms = 0.0;
        int count = 0;
        double current_frame_ms = 0.0;
        double max_frame_ms = 0.0;
    };

    struct value_stat {
        double total_value = 0.0;
        int frame_count = 0;
        double current_frame_value = 0.0;
        bool current_frame_has_value = false;
        double max_frame_value = 0.0;
    };

    std::unordered_map<std::string, section_stat> stats_;
    std::unordered_map<std::string, value_stat> value_stats_;
    int frame_count_ = 0;
    int report_interval_ = 120;
    bool reporting_enabled_ = true;
    report_snapshot last_report_;
    std::vector<report_snapshot> pending_report_history_;
    int file_write_count_ = 0;
    int history_write_interval_ = 20;
    std::filesystem::path latest_report_path_ = std::filesystem::path("logs") / "frame_profiler_latest.log";
    std::filesystem::path history_report_path_ = std::filesystem::path("logs") / "frame_profiler_history.log";

    static inline thread_local frame_profiler* active_profiler_ = nullptr;

    [[nodiscard]] static std::string format_report_text(const report_snapshot& report) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(3);
        stream << "[frame_profiler] average over " << report.frame_count << " frames:\n";

        for (const auto& entry : report.sections) {
            stream << "  " << entry.name << ": " << entry.avg_frame_ms << " ms/frame"
                   << " (avg call " << entry.avg_call_ms << " ms, max frame " << entry.max_frame_ms << " ms)\n";
        }

        for (const auto& entry : report.values) {
            stream << "  " << entry.name << ": "
                   << entry.avg_frame_value
                   << " avg/frame"
                   << " (max frame " << entry.max_frame_value << ")\n";
        }

        return stream.str();
    }

    void write_report_files() {
        if (last_report_.frame_count <= 0)
            return;

        std::error_code error_code;
        if (const auto latest_parent = latest_report_path_.parent_path(); !latest_parent.empty())
            std::filesystem::create_directories(latest_parent, error_code);
        if (const auto history_parent = history_report_path_.parent_path(); !history_parent.empty())
            std::filesystem::create_directories(history_parent, error_code);

        const std::string report_text = format_report_text(last_report_);
        {
            std::ofstream latest_file(latest_report_path_, std::ios::out | std::ios::trunc);
            if (latest_file.is_open())
                latest_file << report_text;
        }

        pending_report_history_.push_back(last_report_);
        if (history_write_interval_ > 0 && static_cast<int>(pending_report_history_.size()) >= history_write_interval_) {
            std::ofstream history_file(history_report_path_, std::ios::out | std::ios::app);
            if (history_file.is_open()) {
                for (const auto& report : pending_report_history_) {
                    history_file << "==================================================\n";
                    history_file << format_report_text(report);
                    history_file << '\n';
                }
            }

            pending_report_history_.clear();
        }

        ++file_write_count_;
    }

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

    static void add_value_active(const std::string& name, double value) {
        if (!active_profiler_)
            return;

        auto& stat = active_profiler_->value_stats_[name];
        stat.current_frame_value = value;
        stat.current_frame_has_value = true;
    }

    void reset() {
        frame_count_ = 0;
        stats_.clear();
        value_stats_.clear();
        last_report_ = {};
        pending_report_history_.clear();
        file_write_count_ = 0;
    }

    void set_reporting_enabled(bool enabled) {
        reporting_enabled_ = enabled;
    }

    [[nodiscard]] const report_snapshot& get_last_report() const {
        return last_report_;
    }

    [[nodiscard]] int get_report_interval() const {
        return report_interval_;
    }

    void end_frame() {
        for (auto& [name, stat] : stats_)
            stat.max_frame_ms = std::max(stat.max_frame_ms, stat.current_frame_ms);

        for (auto& [name, stat] : value_stats_) {
            if (!stat.current_frame_has_value)
                continue;

            stat.total_value += stat.current_frame_value;
            stat.frame_count += 1;
            stat.max_frame_value = std::max(stat.max_frame_value, stat.current_frame_value);
        }

        ++frame_count_;
        if (frame_count_ < report_interval_) {
            for (auto& [name, stat] : stats_)
                stat.current_frame_ms = 0.0;
            for (auto& [name, stat] : value_stats_) {
                stat.current_frame_value = 0.0;
                stat.current_frame_has_value = false;
            }
            return;
        }

        if (!reporting_enabled_) {
            frame_count_ = 0;
            stats_.clear();
            value_stats_.clear();
            return;
        }

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

        std::vector<value_report_entry> value_report_entries;
        value_report_entries.reserve(value_stats_.size());
        for (const auto& [name, stat] : value_stats_) {
            if (stat.frame_count <= 0)
                continue;

            value_report_entries.push_back({
                name,
                stat.total_value / static_cast<double>(stat.frame_count),
                stat.max_frame_value
            });
        }

        std::sort(value_report_entries.begin(), value_report_entries.end(), [](const value_report_entry& lhs, const value_report_entry& rhs) {
            return lhs.avg_frame_value > rhs.avg_frame_value;
        });

        last_report_.frame_count = frame_count_;
        last_report_.sections = report_entries;
        last_report_.values = value_report_entries;

        if (!reporting_enabled_) {
            frame_count_ = 0;
            stats_.clear();
            value_stats_.clear();
            return;
        }

        write_report_files();

        frame_count_ = 0;
        stats_.clear();
        value_stats_.clear();
    }
};