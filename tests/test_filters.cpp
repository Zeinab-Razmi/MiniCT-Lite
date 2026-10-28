#include <gtest/gtest.h>
#include <stdexcept>
#include "minict/filters.hpp"
#include "minict/image.hpp"

using namespace minict;

TEST(Filters, MedianRemovesIsolatedSpike) {
    Image img(5, 5, 0.2f);
    img.at(2, 2) = 1.0f;
    EXPECT_FLOAT_EQ(medianFilter(img, 3).at(2, 2), 0.2f);
    EXPECT_FLOAT_EQ(medianFilter(img, 5).at(2, 2), 0.2f);
}

TEST(Filters, EvenKernelSizeThrows) {
    Image img(5, 5);
    EXPECT_THROW(medianFilter(img, 4), std::invalid_argument);
    EXPECT_THROW(meanFilter(img, 2), std::invalid_argument);
}

TEST(Filters, MeanOfConstantImageIsUnchanged) {
    Image img(8, 8, 0.7f);
    EXPECT_NEAR(meanFilter(img, 3).at(4, 4), 0.7f, 1e-6);
}

TEST(Filters, GaussianKeepsConstantImage) {
    Image img(16, 16, 0.3f);
    EXPECT_NEAR(gaussianFilterSeparable(img, 1.5f).at(8, 8), 0.3f, 1e-5);
}

TEST(Filters, SeparableMatchesNaive) {
    Image img(32, 32);
    for (std::size_t i = 0; i < img.size(); ++i) img[i] = static_cast<float>((i * 37) % 100) / 100.0f;
    Image a = gaussianFilterNaive(img, 1.5f);
    Image b = gaussianFilterSeparable(img, 1.5f);
    for (std::size_t i = 0; i < a.size(); ++i) EXPECT_NEAR(a[i], b[i], 1e-5);
}
