#include "minict/corrections.hpp"
#include <cmath>
#include <stdexcept>

namespace minict {

namespace {
void checkSameSize(const Image& a, const Image& b) {
    if (a.width() != b.width() || a.height() != b.height())
        throw std::invalid_argument("Images must have the same size");
}
}  // namespace

Image offsetCorrection(const Image& raw, const Image& dark) {
    checkSameSize(raw, dark);
    Image out(raw.width(), raw.height());
    for (std::size_t i = 0; i < raw.size(); ++i) out[i] = raw[i] - dark[i];
    return out;
}

Image flatFieldCorrection(const Image& raw, const Image& dark, const Image& flat, float minGain) {
    checkSameSize(raw, dark);
    checkSameSize(raw, flat);
    if (!std::isfinite(minGain) || minGain <= 0.0f) throw std::invalid_argument("minGain must be positive");
    Image out(raw.width(), raw.height());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        float gain = flat[i] - dark[i];
        // A tiny, zero or negative response is an invalid measurement, not a gain of 1.
        out[i] = (gain < minGain) ? 0.0f : (raw[i] - dark[i]) / gain;
    }
    return out;
}

std::vector<std::size_t> findDeadPixels(const Image& dark, const Image& flat, float threshold) {
    checkSameSize(dark, flat);
    std::vector<std::size_t> dead;
    for (std::size_t i = 0; i < flat.size(); ++i)
        if (flat[i] - dark[i] < threshold) dead.push_back(i);
    return dead;
}

Image fixDeadPixels(const Image& img, const std::vector<std::size_t>& dead) {
    Image out = img;
    std::vector<bool> isDead(img.size(), false);
    for (std::size_t p : dead) {
        if (p >= img.size()) throw std::out_of_range("Dead pixel index is outside the image");
        isDead[p] = true;
    }

    const int w = img.width(), h = img.height();
    for (std::size_t p : dead) {
        int x = static_cast<int>(p % w), y = static_cast<int>(p / w);
        float sum = 0.0f;
        int count = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx, ny = y + dy;
                if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                if (isDead[static_cast<std::size_t>(ny) * w + nx]) continue;
                sum += img.at(nx, ny);
                ++count;
            }
        }
        if (count > 0) out.at(x, y) = sum / count;
    }
    return out;
}

}  // namespace minict
