#pragma once
#include <string>
#include "minict/image.hpp"

namespace minict {

// Window/level: which range of the raw values is shown from black to white.
struct Window {
    float center = 0.0f;
    float width = 1.0f;
};

struct DicomData {
    Image image;          // pixel values after RescaleSlope/Intercept (e.g. Hounsfield units for CT)
    bool hasWindow = false;
    Window window;        // the window stored in the file (WindowCenter / WindowWidth), if any
};

// Reads a grayscale DICOM file (8 or 16 bit) using the DCMTK library.
// Uncompressed and JPEG-compressed files are supported. For multi-frame
// files only the first frame is read. Throws std::runtime_error on failure.
DicomData loadDICOM(const std::string& path);

// Window with the smallest and largest value of the image (used if the file has none).
Window minMaxWindow(const Image& img);

// Maps the values inside the window to 0..1 (below -> 0, above -> 1).
Image applyWindow(const Image& img, Window w);

}  // namespace minict
