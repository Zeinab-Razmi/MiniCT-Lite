#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <set>
#include <stdexcept>
#include "minict/filters.hpp"
#include "minict/image.hpp"
#include "minict/metrics.hpp"
#include "minict/phantom.hpp"
#include "minict/png.hpp"

using namespace minict;

// ---------------- Image ----------------
TEST(ImageTest, StoresSizeAndFillValue) {
    Image img(4, 3, 0.25f);
    EXPECT_EQ(img.width(), 4);
    EXPECT_EQ(img.height(), 3);
    EXPECT_EQ(img.size(), 12u);
    EXPECT_FLOAT_EQ(img.at(3, 2), 0.25f);
}

TEST(ImageTest, IndexingIsRowByRow) {
    Image img(3, 2);
    img.at(2, 1) = 7.0f;
    EXPECT_FLOAT_EQ(img[1 * 3 + 2], 7.0f);
}

TEST(ImageTest, ClampedAccessUsesBorderPixels) {
    Image img(2, 2);
    img.at(0, 0) = 1.0f;
    img.at(1, 1) = 4.0f;
    EXPECT_FLOAT_EQ(img.atClamped(-5, -5), 1.0f);
    EXPECT_FLOAT_EQ(img.atClamped(9, 9), 4.0f);
}

TEST(ImageTest, InvalidSizeThrows) {
    EXPECT_THROW(Image(0, 5), std::invalid_argument);
    EXPECT_THROW(Image(5, -1), std::invalid_argument);
}

TEST(ImageTest, DefaultImageIsEmpty) {
    EXPECT_EQ(Image().size(), 0u);
}

// ---------------- Filters: parameters and borders ----------------
TEST(FilterParams, InvalidSigmaThrows) {
    Image img(8, 8, 0.5f);
    EXPECT_THROW(gaussianFilterSeparable(img, 0.0f), std::invalid_argument);
    EXPECT_THROW(gaussianFilterSeparable(img, -1.0f), std::invalid_argument);
    EXPECT_THROW(gaussianFilterSeparable(img, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(gaussianFilterSeparable(img, std::numeric_limits<float>::infinity()), std::invalid_argument);
    EXPECT_THROW(gaussianFilterNaive(img, 1e9f), std::invalid_argument);
}

TEST(FilterParams, ImpulseSpreadsSymmetricallyAndKeepsItsSum) {
    Image img(21, 21, 0.0f);
    img.at(10, 10) = 1.0f;
    Image out = gaussianFilterSeparable(img, 1.5f);
    double sum = 0.0;
    for (std::size_t i = 0; i < out.size(); ++i) sum += out[i];
    EXPECT_NEAR(sum, 1.0, 1e-4);
    EXPECT_NEAR(out.at(8, 10), out.at(12, 10), 1e-7);
    EXPECT_NEAR(out.at(10, 8), out.at(10, 12), 1e-7);
    EXPECT_GT(out.at(10, 10), out.at(11, 10));
}

TEST(FilterParams, BorderPixelsAreHandled) {
    Image img(5, 5, 0.4f);
    EXPECT_NEAR(meanFilter(img, 5).at(0, 0), 0.4f, 1e-6);
    EXPECT_NEAR(medianFilter(img, 5).at(4, 4), 0.4f, 1e-6);
    EXPECT_NEAR(gaussianFilterNaive(img, 2.0f).at(0, 4), 0.4f, 1e-5);
}

// ---------------- Metrics ----------------
TEST(MetricsTest, KnownMseAndPsnr) {
    Image a(2, 2, 0.0f), b(2, 2, 0.1f);
    EXPECT_NEAR(mse(a, b), 0.01, 1e-7);
    EXPECT_NEAR(psnr(a, b, 1.0), 20.0, 1e-4);  // 10*log10(1 / 0.01)
}

TEST(MetricsTest, InvalidInputsThrow) {
    Image a(2, 2), b(3, 3);
    EXPECT_THROW(mse(a, b), std::invalid_argument);
    EXPECT_THROW(mse(Image(), Image()), std::invalid_argument);
    EXPECT_THROW(psnr(a, a, 0.0), std::invalid_argument);
    EXPECT_THROW(psnr(a, a, -1.0), std::invalid_argument);
    EXPECT_THROW(psnr(a, a, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}

// ---------------- PNG output ----------------
TEST(PngTest, WritesAFile) {
    Image img(8, 8, 0.5f);
    std::string path = (std::filesystem::temp_directory_path() / "minict_test.png").string();
    std::filesystem::remove(path);
    savePNG(img, path);
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 0u);
}

TEST(PngTest, InvalidInputThrows) {
    Image img(4, 4, 0.5f);
    std::string path = (std::filesystem::temp_directory_path() / "minict_bad.png").string();
    EXPECT_THROW(savePNG(img, path, 1.0f, 1.0f), std::invalid_argument);
    EXPECT_THROW(savePNG(img, path, 1.0f, 0.0f), std::invalid_argument);
    EXPECT_THROW(savePNG(Image(), path), std::invalid_argument);
    EXPECT_THROW(savePNG(img, "/this/folder/does/not/exist/x.png"), std::runtime_error);
}

TEST(PngTest, NonFiniteValuesDoNotCrash) {
    Image img(2, 2, 0.5f);
    img.at(0, 0) = std::numeric_limits<float>::quiet_NaN();
    img.at(1, 1) = std::numeric_limits<float>::infinity();
    std::string path = (std::filesystem::temp_directory_path() / "minict_nan.png").string();
    EXPECT_NO_THROW(savePNG(img, path));
}

// ---------------- Detector simulation ----------------
TEST(Simulation, DeadPixelsAreUniqueAndCounted) {
    Image truth = makePhantom(32);
    RawAcquisition acq = simulateDetector(truth, 0.05f, 200, 7);
    EXPECT_EQ(acq.deadPixels.size(), 200u);
    std::set<std::size_t> unique(acq.deadPixels.begin(), acq.deadPixels.end());
    EXPECT_EQ(unique.size(), 200u);
}

TEST(Simulation, InvalidParametersThrow) {
    Image truth = makePhantom(16);
    EXPECT_THROW(simulateDetector(truth, -0.1f), std::invalid_argument);
    EXPECT_THROW(simulateDetector(truth, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(simulateDetector(truth, 0.05f, -1), std::invalid_argument);
    EXPECT_THROW(simulateDetector(truth, 0.05f, 100000), std::invalid_argument);
    EXPECT_THROW(simulateDetector(Image(1, 1), 0.05f, 0), std::invalid_argument);
    EXPECT_THROW(makePhantom(1), std::invalid_argument);
}

TEST(Simulation, SameSeedGivesSameImage) {
    Image truth = makePhantom(16);
    RawAcquisition a = simulateDetector(truth, 0.05f, 5, 123);
    RawAcquisition b = simulateDetector(truth, 0.05f, 5, 123);
    for (std::size_t i = 0; i < a.raw.size(); ++i) EXPECT_FLOAT_EQ(a.raw[i], b.raw[i]);
}
