#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include "minict/corrections.hpp"
#include "minict/filters.hpp"
#include "minict/metrics.hpp"
#include "minict/phantom.hpp"

using namespace minict;

TEST(Pipeline, PsnrOfIdenticalImagesIsInfinite) {
    Image a(4, 4, 0.5f);
    EXPECT_TRUE(std::isinf(psnr(a, a)));
}

TEST(Pipeline, EachStepImprovesQuality) {
    Image truth = makePhantom(128);
    RawAcquisition acq = simulateDetector(truth);

    Image flatFixed = flatFieldCorrection(acq.raw, acq.dark, acq.flat);
    auto dead = findDeadPixels(acq.dark, acq.flat);
    Image pixelsFixed = fixDeadPixels(flatFixed, dead);
    Image denoised = gaussianFilterSeparable(pixelsFixed, 1.0f);

    double pRaw = psnr(truth, acq.raw);
    double pFlat = psnr(truth, flatFixed);
    double pDead = psnr(truth, pixelsFixed);
    double pDen = psnr(truth, denoised);

    EXPECT_GT(pFlat, pRaw);
    EXPECT_GT(pDead, pFlat);
    EXPECT_GT(pDen, pDead);
}

TEST(Pipeline, AllDeadPixelsAreDetected) {
    Image truth = makePhantom(128);
    RawAcquisition acq = simulateDetector(truth);
    auto found = findDeadPixels(acq.dark, acq.flat);
    for (std::size_t p : acq.deadPixels) {
        EXPECT_NE(std::find(found.begin(), found.end(), p), found.end());
    }
}
