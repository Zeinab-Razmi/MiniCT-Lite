#pragma once
#include <cstddef>
#include <vector>

namespace minict {

// A grayscale image: width * height float values stored row by row.
class Image {
public:
    Image() = default;
    Image(int width, int height, float fill = 0.0f);

    int width() const { return width_; }
    int height() const { return height_; }
    std::size_t size() const { return data_.size(); }

    // at(): NOT bounds-checked (0 <= x < width, 0 <= y < height is the caller's job).
    // Use atClamped() when x/y may lie outside the image.
    float& at(int x, int y) { return data_[index(x, y)]; }
    float at(int x, int y) const { return data_[index(x, y)]; }

    // Same as at(), but coordinates outside the image are clamped to the border.
    float atClamped(int x, int y) const;

    float& operator[](std::size_t i) { return data_[i]; }
    float operator[](std::size_t i) const { return data_[i]; }

private:
    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * width_ + x;
    }
    int width_ = 0;
    int height_ = 0;
    std::vector<float> data_;
};

}  // namespace minict
