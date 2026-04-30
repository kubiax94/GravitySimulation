#include "texture.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

#include <wincodec.h>

namespace {
    struct com_raii {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool should_uninitialize = false;

        com_raii() {
            should_uninitialize = SUCCEEDED(hr);
        }

        ~com_raii() {
            if (should_uninitialize)
                CoUninitialize();
        }
    };

    template<typename T>
    void release_com(T*& ptr) {
        if (ptr) {
            ptr->Release();
            ptr = nullptr;
        }
    }

    std::wstring to_wstring(const std::string& value) {
        if (value.empty())
            return {};

        const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (size <= 0)
            return std::wstring(value.begin(), value.end());

        std::wstring result(static_cast<size_t>(size - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
        return result;
    }

    std::string resolve_texture_path(const std::string& path) {
        if (path.empty())
            return {};

        std::filesystem::path input(path);
        if (std::filesystem::exists(input))
            return input.string();

        auto cwd = std::filesystem::current_path();
        for (int i = 0; i < 6; ++i) {
            const auto candidate = cwd / input;
            if (std::filesystem::exists(candidate))
                return candidate.string();
            if (!cwd.has_parent_path())
                break;
            cwd = cwd.parent_path();
        }

        return {};
    }
}

texture::texture(const std::string& texture_path, const std::string& name)
    : asset(asset_type::TEXTURE, name) {
    set_source_path(texture_path);
}

bool texture::load() {
    pixel_data_.clear();
    width_ = 0;
    height_ = 0;
    channels_ = 4;

    const std::string resolved_path = resolve_texture_path(get_source_path());
    if (resolved_path.empty()) {
        std::cerr << "ERROR::TEXTURE::FILE_NOT_FOUND: " << get_source_path() << std::endl;
        return false;
    }

    com_raii com_scope;
    if (FAILED(com_scope.hr) && com_scope.hr != RPC_E_CHANGED_MODE) {
        std::cerr << "ERROR::TEXTURE::COM_INIT_FAILED: " << std::hex << com_scope.hr << std::dec << std::endl;
        return false;
    }

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    const auto wide_path = to_wstring(resolved_path);
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        std::cerr << "ERROR::TEXTURE::WIC_FACTORY_CREATE_FAILED: " << std::hex << hr << std::dec << std::endl;
        return false;
    }

    hr = factory->CreateDecoderFromFilename(
        wide_path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr)) {
        std::cerr << "ERROR::TEXTURE::DECODER_CREATE_FAILED: " << resolved_path << std::endl;
        release_com(factory);
        return false;
    }

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        std::cerr << "ERROR::TEXTURE::FRAME_LOAD_FAILED: " << resolved_path << std::endl;
        release_com(decoder);
        release_com(factory);
        return false;
    }

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        std::cerr << "ERROR::TEXTURE::CONVERTER_CREATE_FAILED" << std::endl;
        release_com(frame);
        release_com(decoder);
        release_com(factory);
        return false;
    }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        std::cerr << "ERROR::TEXTURE::CONVERTER_INIT_FAILED" << std::endl;
        release_com(converter);
        release_com(frame);
        release_com(decoder);
        release_com(factory);
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        std::cerr << "ERROR::TEXTURE::INVALID_SIZE" << std::endl;
        release_com(converter);
        release_com(frame);
        release_com(decoder);
        release_com(factory);
        return false;
    }

    width_ = static_cast<int>(width);
    height_ = static_cast<int>(height);
    channels_ = 4;
    const UINT stride = width * channels_;
    pixel_data_.resize(static_cast<size_t>(stride) * height);

    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixel_data_.size()), pixel_data_.data());
    if (FAILED(hr)) {
        std::cerr << "ERROR::TEXTURE::PIXEL_COPY_FAILED" << std::endl;
        pixel_data_.clear();
        width_ = 0;
        height_ = 0;
        release_com(converter);
        release_com(frame);
        release_com(decoder);
        release_com(factory);
        return false;
    }

    for (int y = 0; y < height_ / 2; ++y) {
        auto* row_a = pixel_data_.data() + static_cast<size_t>(y) * stride;
        auto* row_b = pixel_data_.data() + static_cast<size_t>(height_ - 1 - y) * stride;
        for (UINT x = 0; x < stride; ++x)
            std::swap(row_a[x], row_b[x]);
    }

    release_com(converter);
    release_com(frame);
    release_com(decoder);
    release_com(factory);
    return true;
}

bool texture::finalize() {
    if (pixel_data_.empty() || width_ <= 0 || height_ <= 0)
        return false;

    const bool allocated = allocate_2d(
        width_,
        height_,
        internal_format_,
        data_format_,
        GL_UNSIGNED_BYTE,
        pixel_data_.data(),
        GL_REPEAT,
        GL_REPEAT,
        GL_LINEAR_MIPMAP_LINEAR,
        GL_LINEAR,
        true);

    if (!allocated)
        return false;

    pixel_data_.clear();
    pixel_data_.shrink_to_fit();
    status_ = resource_status::LOADED;
    return true;
}

bool texture::allocate_2d(
    int width,
    int height,
    GLenum internal_format,
    GLenum data_format,
    GLenum data_type,
    const void* data,
    GLenum wrap_s,
    GLenum wrap_t,
    GLenum min_filter,
    GLenum mag_filter,
    bool generate_mipmaps) {
    if (width <= 0 || height <= 0)
        return false;

    width_ = width;
    height_ = height;
    internal_format_ = internal_format;
    data_format_ = data_format;
    data_type_ = data_type;

    if (id_ == 0)
        glGenTextures(1, &id_);

    glBindTexture(target_, id_);
    glTexParameteri(target_, GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(target_, GL_TEXTURE_WRAP_T, wrap_t);
    glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, mag_filter);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(target_, 0, static_cast<GLint>(internal_format_), width_, height_, 0, data_format_, data_type_, data);
    if (generate_mipmaps)
        glGenerateMipmap(target_);
    glBindTexture(target_, 0);

    status_ = resource_status::LOADED;
    return true;
}

void texture::unload() {
    cleanup();
    pixel_data_.clear();
    pixel_data_.shrink_to_fit();
    width_ = 0;
    height_ = 0;
    channels_ = 4;
    resource::unload();
}

void texture::cleanup() {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
}

bool texture::is_vaild() {
    return id_ != 0 && get_resource_status() == resource_status::LOADED;
}

void texture::bind(GLuint texture_unit) const {
    if (id_ == 0)
        return;

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(target_, id_);
}

void texture::bind_image(GLuint image_unit, GLenum access, GLenum format, GLint level, GLboolean layered, GLint layer) const {
    if (id_ == 0)
        return;

    glBindImageTexture(image_unit, id_, level, layered, layer, access, format);
}

void texture::clear(GLenum format, GLenum type, const void* data) const {
    if (id_ == 0)
        return;

    glClearTexImage(id_, 0, format, type, data);
}

void texture::copy_from_framebuffer(int source_x, int source_y, int width, int height, int destination_x, int destination_y) const {
    if (id_ == 0 || width <= 0 || height <= 0)
        return;

    glBindTexture(target_, id_);
    glCopyTexSubImage2D(target_, 0, destination_x, destination_y, source_x, source_y, width, height);
    glBindTexture(target_, 0);
}
