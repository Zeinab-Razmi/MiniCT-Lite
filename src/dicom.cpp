#include "minict/dicom.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

// DCMTK is the standard open-source C++ library for DICOM
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmjpeg/djdecode.h"

namespace minict {

namespace {
// Registers the JPEG decoders once, so compressed DICOM files can be read too.
struct JpegCodecs {
    JpegCodecs() { DJDecoderRegistration::registerCodecs(); }
    ~JpegCodecs() { DJDecoderRegistration::cleanup(); }
};
}  // namespace

DicomData loadDICOM(const std::string& path) {
    static JpegCodecs codecs;

    DcmFileFormat file;
    OFCondition status = file.loadFile(path.c_str());
    if (status.bad()) throw std::runtime_error("Cannot read DICOM file " + path + ": " + status.text());
    DcmDataset* ds = file.getDataset();

    // If the pixels are compressed, decompress them
    status = ds->chooseRepresentation(EXS_LittleEndianExplicit, nullptr);
    if (status.bad() || !ds->canWriteXfer(EXS_LittleEndianExplicit))
        throw std::runtime_error("Unsupported DICOM compression in " + path);

    // Image description
    // These attributes are required for an image: refuse the file if one is missing.
    Uint16 rows = 0, cols = 0, bits = 0, pixelRep = 0, samples = 0;
    auto require = [&](const DcmTagKey& key, Uint16& value, const char* name) {
        if (ds->findAndGetUint16(key, value).bad())
            throw std::runtime_error(std::string("DICOM attribute missing: ") + name);
    };
    require(DCM_Rows, rows, "Rows");
    require(DCM_Columns, cols, "Columns");
    require(DCM_BitsAllocated, bits, "BitsAllocated");
    require(DCM_PixelRepresentation, pixelRep, "PixelRepresentation");  // 0 = unsigned, 1 = signed
    require(DCM_SamplesPerPixel, samples, "SamplesPerPixel");
    if (rows == 0 || cols == 0) throw std::runtime_error("DICOM file has zero Rows/Columns");

    // MONOCHROME2 = normal grayscale (0 is black). MONOCHROME1 (0 is white) and palette
    // images would need extra handling, so they are rejected instead of shown wrongly.
    OFString photometric;
    if (ds->findAndGetOFString(DCM_PhotometricInterpretation, photometric).bad())
        throw std::runtime_error("DICOM attribute missing: PhotometricInterpretation");
    if (photometric != "MONOCHROME2")
        throw std::runtime_error(std::string("Unsupported PhotometricInterpretation '") + photometric.c_str() +
                                 "' (only MONOCHROME2 is supported)");
    if (samples != 1) throw std::runtime_error("Only grayscale DICOM (1 sample per pixel) is supported");
    if (bits != 8 && bits != 16) throw std::runtime_error("Only 8 or 16 bit DICOM is supported");

    // Rescale (raw value -> real unit, e.g. Hounsfield units) and window stored in the file
    Float64 slope = 1.0, intercept = 0.0, wc = 0.0, ww = 0.0;
    ds->findAndGetFloat64(DCM_RescaleSlope, slope);
    ds->findAndGetFloat64(DCM_RescaleIntercept, intercept);
    bool haveWindow = ds->findAndGetFloat64(DCM_WindowCenter, wc).good() &&
                      ds->findAndGetFloat64(DCM_WindowWidth, ww).good() && ww > 0.0;

    DicomData out;
    out.image = Image(cols, rows);
    const std::size_t n = out.image.size();  // first frame only

    if (bits == 16) {
        const Uint16* px = nullptr;
        unsigned long count = 0;
        if (ds->findAndGetUint16Array(DCM_PixelData, px, &count).bad() || px == nullptr || count < n)
            throw std::runtime_error("DICOM pixel data missing or too short");
        for (std::size_t i = 0; i < n; ++i) {
            double raw = pixelRep == 1 ? static_cast<double>(static_cast<Sint16>(px[i])) : px[i];
            out.image[i] = static_cast<float>(raw * slope + intercept);
        }
    } else {
        const Uint8* px = nullptr;
        unsigned long count = 0;
        if (ds->findAndGetUint8Array(DCM_PixelData, px, &count).bad() || px == nullptr || count < n)
            throw std::runtime_error("DICOM pixel data missing or too short");
        for (std::size_t i = 0; i < n; ++i) {
            double raw = pixelRep == 1 ? static_cast<double>(static_cast<Sint8>(px[i])) : px[i];
            out.image[i] = static_cast<float>(raw * slope + intercept);
        }
    }

    out.hasWindow = haveWindow;
    out.window = {static_cast<float>(wc), static_cast<float>(ww)};
    return out;
}

Window minMaxWindow(const Image& img) {
    if (img.size() == 0) throw std::invalid_argument("Cannot compute a window for an empty image");
    float lo = img[0], hi = img[0];
    for (std::size_t i = 1; i < img.size(); ++i) {
        lo = std::min(lo, img[i]);
        hi = std::max(hi, img[i]);
    }
    if (hi <= lo) hi = lo + 1.0f;
    return {(lo + hi) / 2.0f, hi - lo};
}

Image applyWindow(const Image& img, Window w) {
    if (!std::isfinite(w.center) || !std::isfinite(w.width) || w.width <= 0.0f)
        throw std::invalid_argument("Window must have a finite center and a positive width");
    const float lo = w.center - w.width / 2.0f;
    Image out(img.width(), img.height());
    for (std::size_t i = 0; i < img.size(); ++i) out[i] = std::clamp((img[i] - lo) / w.width, 0.0f, 1.0f);
    return out;
}

}  // namespace minict
