#include "jadefx/scene/image/Image.hpp"

#include "ImageData.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace jadefx {
namespace {

std::vector<unsigned char> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        return {};
    }
    if (static_cast<unsigned long long>(size) > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
        return {};
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (file.gcount() != size) {
        return {};
    }
    return bytes;
}

}  // namespace

Image::Image(std::shared_ptr<ImageData> data) : data_(std::move(data)) {}

Image::~Image() = default;

std::shared_ptr<Image> Image::load(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }
    std::vector<unsigned char> bytes;
    try {
        bytes = ReadFile(std::filesystem::u8path(path));
    } catch (const std::exception&) {
        return nullptr;
    }
    if (bytes.empty()) {
        return nullptr;
    }
    return load(bytes.data(), bytes.size());
}

std::shared_ptr<Image> Image::load(const std::uint8_t* bytes, std::size_t size) {
    if (bytes == nullptr || size == 0 || size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return nullptr;
    }
    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char* decoded =
        stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &components, 4);
    if (decoded == nullptr || width <= 0 || height <= 0) {
        if (decoded != nullptr) {
            stbi_image_free(decoded);
        }
        return nullptr;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixels > (std::numeric_limits<std::size_t>::max() / 4)) {
        stbi_image_free(decoded);
        return nullptr;
    }
    auto data = std::make_shared<ImageData>();
    data->width = width;
    data->height = height;
    data->rgba.assign(decoded, decoded + pixels * 4);
    stbi_image_free(decoded);
    return std::shared_ptr<Image>(new Image(std::move(data)));
}

int Image::getWidth() const { return data_ ? data_->width : 0; }

int Image::getHeight() const { return data_ ? data_->height : 0; }

}  // namespace jadefx
