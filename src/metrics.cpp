#include "minict/metrics.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace minict {

double mse(const Image& a, const Image& b) {
    if (a.width() != b.width() || a.height() != b.height())
        throw std::invalid_argument("Images must have the same size");
    if (a.size() == 0) throw std::invalid_argument("Cannot compute MSE of empty images");
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        double d = static_cast<double>(a[i]) - b[i];
        sum += d * d;
    }
    return sum / static_cast<double>(a.size());
}

double psnr(const Image& reference, const Image& test, double peak) {
    if (!std::isfinite(peak) || peak <= 0.0) throw std::invalid_argument("PSNR peak value must be positive");
    double e = mse(reference, test);
    if (e == 0.0) return std::numeric_limits<double>::infinity();
    return 10.0 * std::log10(peak * peak / e);
}

}  // namespace minict
