#pragma once

struct i_data_provider
{
    virtual ~i_data_provider() = default;
    virtual void* get_buffer_data() const = 0;
    virtual size_t get_buffer_size() const = 0;
    virtual size_t get_buffer_count() const = 0;
};
