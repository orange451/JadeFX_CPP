#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace jadefx {

struct ImageData;
class ImageView;

// Decoded bitmap. load reads PNG, JPEG, GIF, BMP, and the other formats
// stb_image accepts. Empty when the file or the bytes cannot be decoded.
// Several ImageViews share one Image.
class Image {
public:
    static std::shared_ptr<Image> load(const std::string& path);
    static std::shared_ptr<Image> load(const std::uint8_t* bytes, std::size_t size);

    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    int getWidth() const;
    int getHeight() const;

private:
    friend class ImageView;
    explicit Image(std::shared_ptr<ImageData> data);

    std::shared_ptr<ImageData> data_;
};

}  // namespace jadefx
