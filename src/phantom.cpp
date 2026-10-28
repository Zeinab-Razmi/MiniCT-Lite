#include "minict/phantom.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

namespace minict {

namespace {
const float kOffset = 0.05f;  // constant electronic offset of the detector

// Adds `value` to every pixel inside an ellipse (coordinates in -1..1).
void addEllipse(Image& img, float cx, float cy, float rx, float ry, float value) {
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            float u = (2.0f * x / (img.width() - 1) - 1.0f - cx) / rx;
            float v = (2.0f * y / (img.height() - 1) - 1.0f - cy) / ry;
            if (u * u + v * v <= 1.0f) img.at(x, y) += value;
        }
    }
}
}  // namespace

Image makePhantom(int size) {
    if (size < 2) throw std::invalid_argument("Phantom size must be at least 2");
    Image img(size, size, 0.0f);
    addEllipse(img, 0.0f, 0.0f, 0.85f, 0.95f, 0.5f);     // body
    addEllipse(img, 0.0f, 0.0f, 0.75f, 0.85f, -0.2f);    // soft tissue inside
    addEllipse(img, -0.30f, 0.10f, 0.20f, 0.30f, 0.25f); // organ 1
    addEllipse(img, 0.30f, 0.10f, 0.20f, 0.30f, 0.25f);  // organ 2
    addEllipse(img, 0.0f, -0.45f, 0.10f, 0.10f, 0.35f);  // small bright spot
    addEllipse(img, 0.0f, 0.50f, 0.15f, 0.08f, 0.15f);   // small structure
    return img;
}

RawAcquisition simulateDetector(const Image& truth, float noiseSigma,
                                int numDeadPixels, unsigned seed) {
    const int w = truth.width(), h = truth.height();
    if (w < 2 || h < 2) throw std::invalid_argument("Image must be at least 2x2");
    if (!std::isfinite(noiseSigma) || noiseSigma < 0.0f) throw std::invalid_argument("noiseSigma must be >= 0");
    if (numDeadPixels < 0 || static_cast<std::size_t>(numDeadPixels) > truth.size())
        throw std::invalid_argument("numDeadPixels must be between 0 and the number of pixels");
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.0f, noiseSigma);

    RawAcquisition acq{Image(w, h), Image(w, h, kOffset), Image(w, h), {}};

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Gain varies smoothly from 0.8 (left) to 1.2 (right)
            float gain = 0.8f + 0.4f * x / (w - 1);
            acq.flat.at(x, y) = gain * 1.0f + kOffset;          // uniform exposure = 1
            acq.raw.at(x, y) = truth.at(x, y) * gain + kOffset + noise(rng);
        }
    }

    // Dead pixels read 0 in every image. Pick numDeadPixels DIFFERENT pixels
    // (partial shuffle of all indices, so no pixel is chosen twice).
    std::vector<std::size_t> indices(truth.size());
    std::iota(indices.begin(), indices.end(), std::size_t{0});
    for (int i = 0; i < numDeadPixels; ++i) {
        std::uniform_int_distribution<std::size_t> pick(static_cast<std::size_t>(i), indices.size() - 1);
        std::swap(indices[i], indices[pick(rng)]);
        std::size_t p = indices[i];
        acq.raw[p] = 0.0f;
        acq.flat[p] = 0.0f;
        acq.deadPixels.push_back(p);
    }
    return acq;
}

}  // namespace minict
