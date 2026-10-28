#include <gtest/gtest.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include "minict/dicom.hpp"

using namespace minict;

namespace {

// ---- helpers that write a tiny DICOM file (Explicit VR Little Endian) ----
void put16(std::string& s, std::uint16_t v) { s.push_back(char(v & 0xFF)); s.push_back(char(v >> 8)); }
void put32(std::string& s, std::uint32_t v) { put16(s, v & 0xFFFF); put16(s, v >> 16); }

void tag(std::string& s, std::uint16_t g, std::uint16_t e, const char* vr) {
    put16(s, g); put16(s, e); s += vr;
}
void elemUS(std::string& s, std::uint16_t g, std::uint16_t e, std::uint16_t v) {
    tag(s, g, e, "US"); put16(s, 2); put16(s, v);
}
void elemText(std::string& s, std::uint16_t g, std::uint16_t e, const char* vr, std::string v, char pad = ' ') {
    if (v.size() % 2) v.push_back(pad);
    tag(s, g, e, vr); put16(s, static_cast<std::uint16_t>(v.size())); s += v;
}

// Options to build valid or deliberately broken files
struct Opts {
    std::string transferSyntax = "1.2.840.10008.1.2.1";
    std::string photometric = "MONOCHROME2";
    bool includePixelRepresentation = true;
    std::uint16_t pixelRepresentation = 0;
    std::uint16_t lastPixel = 3500;  // value of the 8th pixel
};

std::string makeDicom(const Opts& o = Opts()) {
    std::string s(128, '\0');
    s += "DICM";
    elemText(s, 0x0002, 0x0010, "UI", o.transferSyntax, '\0');
    // A sequence of undefined length that the reader must skip correctly
    tag(s, 0x0008, 0x1140, "SQ"); put16(s, 0); put32(s, 0xFFFFFFFF);
    put16(s, 0xFFFE); put16(s, 0xE000); put32(s, 0xFFFFFFFF);          // item, undefined length
    elemText(s, 0x0008, 0x1150, "UI", "1.2.3", '\0');
    put16(s, 0xFFFE); put16(s, 0xE00D); put32(s, 0);                   // item delimiter
    put16(s, 0xFFFE); put16(s, 0xE0DD); put32(s, 0);                   // sequence delimiter
    // Image description
    elemUS(s, 0x0028, 0x0002, 1);
    elemText(s, 0x0028, 0x0004, "CS", o.photometric);
    elemUS(s, 0x0028, 0x0010, 2);   // rows
    elemUS(s, 0x0028, 0x0011, 4);   // columns
    elemUS(s, 0x0028, 0x0100, 16);  // bits allocated
    if (o.includePixelRepresentation) elemUS(s, 0x0028, 0x0103, o.pixelRepresentation);
    elemText(s, 0x0028, 0x1050, "DS", "300\\999");  // two values: only the first is used
    elemText(s, 0x0028, 0x1051, "DS", "600");
    elemText(s, 0x0028, 0x1052, "DS", "-1024");
    elemText(s, 0x0028, 0x1053, "DS", "1");
    // Pixel data: 8 values 0, 500, ..., 3000, lastPixel
    tag(s, 0x7FE0, 0x0010, "OW"); put16(s, 0); put32(s, 16);
    for (int i = 0; i < 7; ++i) put16(s, static_cast<std::uint16_t>(i * 500));
    put16(s, o.lastPixel);
    return s;
}

std::string writeTemp(const std::string& bytes, const char* name) {
    std::string path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream(path, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return path;
}

}  // namespace

TEST(Dicom, ReadsSizeAndAppliesRescale) {
    DicomData d = loadDICOM(writeTemp(makeDicom(), "minict_ok.dcm"));
    EXPECT_EQ(d.image.width(), 4);
    EXPECT_EQ(d.image.height(), 2);
    EXPECT_FLOAT_EQ(d.image.at(0, 0), -1024.0f);           // 0 * 1 - 1024
    EXPECT_FLOAT_EQ(d.image.at(3, 1), 3500.0f - 1024.0f);  // 3500 * 1 - 1024
}

TEST(Dicom, ReadsWindowFromFile) {
    DicomData d = loadDICOM(writeTemp(makeDicom(), "minict_win.dcm"));
    ASSERT_TRUE(d.hasWindow);
    EXPECT_FLOAT_EQ(d.window.center, 300.0f);
    EXPECT_FLOAT_EQ(d.window.width, 600.0f);
}

TEST(Dicom, SignedPixelsKeepTheirSign) {
    Opts o;
    o.pixelRepresentation = 1;  // signed
    o.lastPixel = 0xFFFF;       // -1 as a signed 16-bit value
    DicomData d = loadDICOM(writeTemp(makeDicom(o), "minict_signed.dcm"));
    EXPECT_FLOAT_EQ(d.image.at(3, 1), -1.0f - 1024.0f);
}

TEST(Dicom, UnsignedPixelsStayPositive) {
    Opts o;
    o.lastPixel = 0xFFFF;  // 65535 when unsigned
    DicomData d = loadDICOM(writeTemp(makeDicom(o), "minict_unsigned.dcm"));
    EXPECT_FLOAT_EQ(d.image.at(3, 1), 65535.0f - 1024.0f);
}

TEST(Dicom, MissingPixelRepresentationThrows) {
    Opts o;
    o.includePixelRepresentation = false;
    EXPECT_THROW(loadDICOM(writeTemp(makeDicom(o), "minict_nopr.dcm")), std::runtime_error);
}

TEST(Dicom, Monochrome1IsRejected) {
    Opts o;
    o.photometric = "MONOCHROME1";
    EXPECT_THROW(loadDICOM(writeTemp(makeDicom(o), "minict_mono1.dcm")), std::runtime_error);
}

TEST(Dicom, FileWithoutDicmMarkerThrows) {
    EXPECT_THROW(loadDICOM(writeTemp(std::string(200, 'x'), "minict_bad.dcm")), std::runtime_error);
}

TEST(Dicom, MissingFileThrows) {
    EXPECT_THROW(loadDICOM("/this/does/not/exist.dcm"), std::runtime_error);
}

TEST(Window, MapsWindowRangeToZeroOne) {
    Image img(3, 1);
    img.at(0, 0) = 0.0f;    // bottom of window (300 - 600/2 = 0)
    img.at(1, 0) = 300.0f;  // centre
    img.at(2, 0) = 900.0f;  // above the window
    Image out = applyWindow(img, {300.0f, 600.0f});
    EXPECT_FLOAT_EQ(out.at(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(out.at(1, 0), 0.5f);
    EXPECT_FLOAT_EQ(out.at(2, 0), 1.0f);
}

TEST(Window, InvalidWindowThrows) {
    Image img(2, 2, 1.0f);
    EXPECT_THROW(applyWindow(img, {0.0f, 0.0f}), std::invalid_argument);
    EXPECT_THROW(applyWindow(img, {0.0f, std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
}

TEST(Window, MinMaxWindowCoversAllValues) {
    Image img(2, 1);
    img.at(0, 0) = 10.0f;
    img.at(1, 0) = 30.0f;
    Window w = minMaxWindow(img);
    EXPECT_FLOAT_EQ(w.center, 20.0f);
    EXPECT_FLOAT_EQ(w.width, 20.0f);
}

TEST(Window, MinMaxWindowOfEmptyImageThrows) {
    EXPECT_THROW(minMaxWindow(Image()), std::invalid_argument);
}
