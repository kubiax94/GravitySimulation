#pragma once

class collider;

struct broadphase_pair
{
    collider* first = nullptr;
    collider* second = nullptr;

    [[nodiscard]] bool is_valid() const {
        return first != nullptr && second != nullptr && first != second;
    }
};
