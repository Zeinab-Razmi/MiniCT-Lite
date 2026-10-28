#pragma once
#include <string>
#include "minict/image.hpp"

namespace minict {

// Saves the image as an 8-bit grayscale PNG (opens in any viewer or browser).
// Values between lo and hi are mapped to 0..255 (window/level).
void savePNG(const Image& img, const std::string& path, float lo = 0.0f, float hi = 1.0f);

}  // namespace minict
