#pragma once
#include <cstddef>
#include <vector>
#include "minict/image.hpp"

namespace minict {

// Test image: a big ellipse with a few smaller shapes inside. Values 0..1.
Image makePhantom(int size);

// Detector "calibration" data that a real X-ray system would measure.
struct RawAcquisition {
    Image raw;                         // what the detector gives us (with defects)
    Image dark;                        // image taken with no X-ray (offset only)
    Image flat;                        // image of a uniform exposure (shows gain differences)
    std::vector<std::size_t> deadPixels;  // ground truth (for tests only)
};

// Simulates defects: offset, uneven gain, noise, dead pixels.
RawAcquisition simulateDetector(const Image& truth, float noiseSigma = 0.05f,
                                int numDeadPixels = 30, unsigned seed = 42);

}  // namespace minict
