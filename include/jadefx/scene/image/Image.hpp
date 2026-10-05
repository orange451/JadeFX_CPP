#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace jadefx {

struct ImageData;
class ImageView;
class Node;

// Decoded bitmap. load reads PNG, JPEG, GIF, BMP, and the other formats
// stb_image accepts. Empty when the file or the bytes cannot be decoded.
// Several ImageViews share one Image.
class Image {
public:
    static std::shared_ptr<Image> load(const std::string& path);
    static std::shared_ptr<Image> load(const std::uint8_t* bytes, std::size_t size);
    // An image of width x height RGBA pixels, 8 bits a channel, top row first.
    // Empty when the size is not positive or rgba does not hold width * height * 4 bytes.
    // Safe on any thread: nothing reaches the GPU until a view draws it.
    static std::shared_ptr<Image> fromRgba(int width, int height, std::vector<std::uint8_t> rgba);

    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    int getWidth() const;
    int getHeight() const;

private:
    friend class ImageView;
    friend class Node;
    explicit Image(std::shared_ptr<ImageData> data);

    std::shared_ptr<ImageData> data_;
};

}  // namespace jadefx
