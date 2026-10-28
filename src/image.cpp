#include "minict/image.hpp"
#include <algorithm>
#include <stdexcept>

namespace minict {

Image::Image(int width, int height, float fill) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("Image size must be positive");
    data_.assign(static_cast<std::size_t>(width) * height, fill);
}

float Image::atClamped(int x, int y) const {
    x = std::clamp(x, 0, width_ - 1);
    y = std::clamp(y, 0, height_ - 1);
    return at(x, y);
}

}  // namespace minict
