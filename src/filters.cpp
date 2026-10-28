#include "minict/filters.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace minict {

namespace {
void checkOdd(int size) {
    if (size < 1 || size % 2 == 0) throw std::invalid_argument("Kernel size must be odd and >= 1");
}

// 1D Gaussian kernel, normalised so that the sum is 1.
std::vector<float> gaussianKernel(float sigma) {
    if (!std::isfinite(sigma) || sigma <= 0.0f || sigma > 50.0f)
        throw std::invalid_argument("sigma must be a finite number in (0, 50]");
    int radius = static_cast<int>(std::ceil(3.0f * sigma));
    std::vector<float> k(2 * radius + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i) {
        k[i + radius] = std::exp(-(i * i) / (2.0f * sigma * sigma));
        sum += k[i + radius];
    }
    for (float& v : k) v /= sum;
    return k;
}
}  // namespace

Image meanFilter(const Image& img, int size) {
    checkOdd(size);
    int r = size / 2;
    Image out(img.width(), img.height());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            float sum = 0.0f;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) sum += img.atClamped(x + dx, y + dy);
            out.at(x, y) = sum / static_cast<float>(size * size);
        }
    return out;
}

Image medianFilter(const Image& img, int size) {
    checkOdd(size);
    int r = size / 2;
    Image out(img.width(), img.height());
    std::vector<float> window(static_cast<std::size_t>(size) * size);
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            std::size_t n = 0;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) window[n++] = img.atClamped(x + dx, y + dy);
            std::nth_element(window.begin(), window.begin() + n / 2, window.end());
            out.at(x, y) = window[n / 2];
        }
    return out;
}

Image gaussianFilterNaive(const Image& img, float sigma) {
    std::vector<float> k = gaussianKernel(sigma);
    int r = static_cast<int>(k.size() / 2);
    Image out(img.width(), img.height());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            float sum = 0.0f;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                    sum += k[dy + r] * k[dx + r] * img.atClamped(x + dx, y + dy);
            out.at(x, y) = sum;
        }
    return out;
}

Image gaussianFilterSeparable(const Image& img, float sigma) {
    std::vector<float> k = gaussianKernel(sigma);
    int r = static_cast<int>(k.size() / 2);
    Image tmp(img.width(), img.height());
    Image out(img.width(), img.height());
    // Pass 1: horizontal
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            float sum = 0.0f;
            for (int d = -r; d <= r; ++d) sum += k[d + r] * img.atClamped(x + d, y);
            tmp.at(x, y) = sum;
        }
    // Pass 2: vertical
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            float sum = 0.0f;
            for (int d = -r; d <= r; ++d) sum += k[d + r] * tmp.atClamped(x, y + d);
            out.at(x, y) = sum;
        }
    return out;
}

}  // namespace minict
