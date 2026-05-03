#pragma once

struct ui_spacing {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    ui_spacing() = default;
    explicit ui_spacing(float uniform)
        : left(uniform), top(uniform), right(uniform), bottom(uniform) {
    }

    ui_spacing(float horizontal, float vertical)
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {
    }

    ui_spacing(float left_value, float top_value, float right_value, float bottom_value)
        : left(left_value), top(top_value), right(right_value), bottom(bottom_value) {
    }
};

enum class ui_horizontal_alignment {
    left,
    center,
    right
};

enum class ui_vertical_alignment {
    top,
    center,
    bottom
};

enum class ui_layout_direction {
    vertical,
    horizontal
};
