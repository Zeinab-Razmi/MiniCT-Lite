#pragma once
#include "minict/image.hpp"

namespace minict {

// Mean squared error between two images of the same size.
double mse(const Image& a, const Image& b);

// Peak signal-to-noise ratio in dB (higher = closer to the reference).
// `peak` is the maximum possible value (1.0 for our images).
double psnr(const Image& reference, const Image& test, double peak = 1.0);

}  // namespace minict
