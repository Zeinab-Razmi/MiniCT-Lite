#pragma once
#include <cstddef>
#include <vector>
#include "minict/image.hpp"

namespace minict {

// Smallest detector response (flat - dark) that counts as a working pixel.
// Used both to find dead pixels and to skip them in the flat-field correction.
constexpr float kMinGain = 0.5f;

// raw - dark
Image offsetCorrection(const Image& raw, const Image& dark);

// (raw - dark) / (flat - dark): removes the gain differences between pixels.
// Invalid pixels, where the response (flat - dark) is below minGain, cannot be
// corrected: they are set to 0 and should be repaired with fixDeadPixels().
Image flatFieldCorrection(const Image& raw, const Image& dark, const Image& flat,
                          float minGain = kMinGain);

// A pixel is "dead" if its response (flat - dark) is below the threshold.
std::vector<std::size_t> findDeadPixels(const Image& dark, const Image& flat,
                                        float threshold = kMinGain);

// Replaces each listed pixel by the average of its valid neighbours (3x3).
// Throws std::out_of_range if an index is not inside the image.
Image fixDeadPixels(const Image& img, const std::vector<std::size_t>& dead);

}  // namespace minict
