#include "minict/png.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

// stb_image_write is a single-file library that writes PNG files.
// It is downloaded automatically by CMake.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace minict {

void savePNG(const Image& img, const std::string& path, float lo, float hi) {
    if (img.size() == 0) throw std::invalid_argument("Cannot save an empty image");
    if (!std::isfinite(lo) || !std::isfinite(hi) || !(hi > lo))
        throw std::invalid_argument("Intensity range must satisfy lo < hi");
    std::vector<unsigned char> pixels(img.size());
    for (std::size_t i = 0; i < img.size(); ++i) {
        float v = std::isfinite(img[i]) ? std::clamp((img[i] - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
        pixels[i] = static_cast<unsigned char>(v * 255.0f + 0.5f);
    }
    // 1 channel = grayscale; the last argument is the number of bytes per row
    if (!stbi_write_png(path.c_str(), img.width(), img.height(), 1, pixels.data(), img.width()))
        throw std::runtime_error("Cannot write PNG file: " + path);
}

}  // namespace minict
