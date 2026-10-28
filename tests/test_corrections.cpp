#include <gtest/gtest.h>
#include <stdexcept>
#include "minict/corrections.hpp"
#include "minict/image.hpp"

using namespace minict;

TEST(Corrections, OffsetIsSubtracted) {
    Image raw(2, 2, 0.5f), dark(2, 2, 0.1f);
    Image out = offsetCorrection(raw, dark);
    EXPECT_FLOAT_EQ(out.at(1, 1), 0.4f);
}

TEST(Corrections, FlatFieldRemovesGain) {
    Image dark(2, 1, 0.0f);
    Image flat(2, 1);
    flat.at(0, 0) = 1.0f;  // gain 1.0
    flat.at(1, 0) = 2.0f;  // gain 2.0
    Image raw(2, 1);
    raw.at(0, 0) = 0.5f;   // true signal 0.5
    raw.at(1, 0) = 1.0f;   // true signal 0.5, doubled by gain
    Image out = flatFieldCorrection(raw, dark, flat);
    EXPECT_FLOAT_EQ(out.at(0, 0), 0.5f);
    EXPECT_FLOAT_EQ(out.at(1, 0), 0.5f);
}

TEST(Corrections, DifferentSizesThrow) {
    EXPECT_THROW(offsetCorrection(Image(2, 2), Image(3, 3)), std::invalid_argument);
}

TEST(Corrections, DeadPixelIsReplacedByNeighbourAverage) {
    Image img(3, 3, 1.0f);
    img.at(1, 1) = 0.0f;  // dead pixel in the middle
    Image out = fixDeadPixels(img, {4});  // index 4 = (1,1)
    EXPECT_FLOAT_EQ(out.at(1, 1), 1.0f);
}

TEST(Corrections, FindsDeadPixels) {
    Image dark(3, 1, 0.0f), flat(3, 1, 1.0f);
    flat.at(2, 0) = 0.0f;
    auto dead = findDeadPixels(dark, flat);
    ASSERT_EQ(dead.size(), 1u);
    EXPECT_EQ(dead[0], 2u);
}

TEST(Corrections, FlatFieldDoesNotInventValuesForDeadPixels) {
    // Dead pixel: raw = 0, flat = 0, but dark = 0.05. A naive (raw-dark)/(flat-dark) gives 1.0.
    Image raw(2, 1, 0.5f), dark(2, 1, 0.05f), flat(2, 1, 1.05f);
    raw.at(1, 0) = 0.0f;
    flat.at(1, 0) = 0.0f;
    Image out = flatFieldCorrection(raw, dark, flat);
    EXPECT_NEAR(out.at(0, 0), 0.45f, 1e-5);
    EXPECT_FLOAT_EQ(out.at(1, 0), 0.0f);  // marked invalid, not 1.0
}

TEST(Corrections, FlatFieldAndDeadPixelDetectionUseTheSameRule) {
    Image dark(3, 1, 0.0f), flat(3, 1, 1.0f), raw(3, 1, 0.6f);
    flat.at(1, 0) = 0.3f;   // response below kMinGain
    flat.at(2, 0) = -0.2f;  // negative response
    auto dead = findDeadPixels(dark, flat);
    Image out = flatFieldCorrection(raw, dark, flat);
    ASSERT_EQ(dead.size(), 2u);
    EXPECT_FLOAT_EQ(out.at(1, 0), 0.0f);
    EXPECT_FLOAT_EQ(out.at(2, 0), 0.0f);
    EXPECT_FLOAT_EQ(out.at(0, 0), 0.6f);
}

TEST(Corrections, FlatFieldInvalidMinGainThrows) {
    Image a(2, 2, 1.0f);
    EXPECT_THROW(flatFieldCorrection(a, a, a, 0.0f), std::invalid_argument);
}

TEST(Corrections, DeadPixelIndexOutsideImageThrows) {
    Image img(3, 3, 1.0f);
    EXPECT_THROW(fixDeadPixels(img, {9}), std::out_of_range);
}

TEST(Corrections, DeadPixelWithAllNeighboursDeadIsLeftUnchanged) {
    Image img(2, 2, 0.5f);
    Image out = fixDeadPixels(img, {0, 1, 2, 3});
    EXPECT_FLOAT_EQ(out.at(0, 0), 0.5f);
}

TEST(Corrections, DeadPixelAtCornerUsesOnlyExistingNeighbours) {
    Image img(3, 3, 1.0f);
    img.at(0, 0) = 0.0f;
    EXPECT_FLOAT_EQ(fixDeadPixels(img, {0}).at(0, 0), 1.0f);
}
