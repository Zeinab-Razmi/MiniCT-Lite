#pragma once
#include "minict/image.hpp"

namespace minict {

// Average of a (size x size) neighbourhood. size must be odd.
Image meanFilter(const Image& img, int size);

// Median of a (size x size) neighbourhood. size must be odd (3, 5, 7...).
// Good against isolated bad pixels ("salt and pepper" noise).
Image medianFilter(const Image& img, int size);

// Gaussian blur, straightforward 2D version: slow, O(N * k^2).
Image gaussianFilterNaive(const Image& img, float sigma);

// Gaussian blur, separable version (rows then columns): same result, O(N * k).
Image gaussianFilterSeparable(const Image& img, float sigma);

}  // namespace minict
