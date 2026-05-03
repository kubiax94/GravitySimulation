#pragma once

#include <cstddef>
#include <string>

struct ui_loading_feedback_state {
    bool active = false;
    bool failed = false;
    std::string title;
    std::string status;
    std::string progress_text;
    float progress = 0.0f;
    size_t completed_resources = 0;
    size_t total_resources = 0;
};
