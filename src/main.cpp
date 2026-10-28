#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
#include "minict/corrections.hpp"
#include "minict/dicom.hpp"
#include "minict/filters.hpp"
#include "minict/metrics.hpp"
#include "minict/png.hpp"
#include "minict/phantom.hpp"

using namespace minict;

int main(int argc, char** argv) {
  try {
    // 1. Get the reference image: a real DICOM file if given, otherwise a built-in test phantom
    Image truth;
    if (argc > 1) {
        DicomData dcm = loadDICOM(argv[1]);
        Window w = dcm.hasWindow ? dcm.window : minMaxWindow(dcm.image);
        truth = applyWindow(dcm.image, w);  // window/level: raw values -> 0..1
        std::printf("Loaded %s: %dx%d, window center %.0f / width %.0f%s\n\n", argv[1],
                    truth.width(), truth.height(), w.center, w.width,
                    dcm.hasWindow ? " (from file)" : " (min/max)");
    } else {
        truth = makePhantom(512);
        std::printf("No DICOM file given: using the built-in phantom.\n"
                    "Usage: minict_cli image.dcm\n\n");
    }
    const int size = truth.width();

    // Simulate a defective detector on top of it (offset, gain, noise, dead pixels)
    RawAcquisition acq = simulateDetector(truth);

    // 2. Corrections
    Image offsetFixed = offsetCorrection(acq.raw, acq.dark);
    Image flatFixed = flatFieldCorrection(acq.raw, acq.dark, acq.flat);
    auto dead = findDeadPixels(acq.dark, acq.flat);
    Image pixelsFixed = fixDeadPixels(flatFixed, dead);

    // 3. Denoising
    Image denoised = gaussianFilterSeparable(pixelsFixed, 1.0f);

    // 4. Quality check against the perfect image
    std::printf("Dead pixels found: %zu\n\n", dead.size());
    std::printf("%-28s %8s\n", "Step", "PSNR(dB)");
    std::printf("%-28s %8.2f\n", "Raw", psnr(truth, acq.raw));
    std::printf("%-28s %8.2f\n", "+ offset correction", psnr(truth, offsetFixed));
    std::printf("%-28s %8.2f\n", "+ flat-field correction", psnr(truth, flatFixed));
    std::printf("%-28s %8.2f\n", "+ dead pixel fix", psnr(truth, pixelsFixed));
    std::printf("%-28s %8.2f\n", "+ Gaussian filter", psnr(truth, denoised));

    // 5. Timing: naive vs separable Gaussian. One warm-up run, then the median of 10 runs.
    auto medianMs = [](auto fn) {
        fn();  // warm-up (fills caches, so the first run does not distort the result)
        std::vector<double> t;
        for (int run = 0; run < 10; ++run) {
            auto t0 = std::chrono::steady_clock::now();
            Image r = fn();
            auto t1 = std::chrono::steady_clock::now();
            if (r.size() == 0) return -1.0;  // never happens; keeps the result "used"
            t.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        std::sort(t.begin(), t.end());
        return t[t.size() / 2];
    };
    const float sigma = 2.0f;
    double tNaive = medianMs([&] { return gaussianFilterNaive(pixelsFixed, sigma); });
    double tSep = medianMs([&] { return gaussianFilterSeparable(pixelsFixed, sigma); });

    // Both versions must give the same image, otherwise the comparison is meaningless
    Image a1 = gaussianFilterNaive(pixelsFixed, sigma), a2 = gaussianFilterSeparable(pixelsFixed, sigma);
    float maxDiff = 0.0f;
    for (std::size_t i = 0; i < a1.size(); ++i) maxDiff = std::max(maxDiff, std::fabs(a1[i] - a2[i]));
    std::printf("\nGaussian (sigma=%.0f) on %dx%d, median of 10 runs: naive %.2f ms, separable %.2f ms (%.1fx faster)\n",
                sigma, size, truth.height(), tNaive, tSep, tNaive / tSep);
    std::printf("Largest pixel difference between the two versions: %.2g\n", maxDiff);

    // 6. Save images 
    savePNG(truth, "1_truth.png");
    savePNG(acq.raw, "2_raw.png");
    savePNG(pixelsFixed, "3_corrected.png");
    savePNG(denoised, "4_denoised.png");
    // 7. Error images: |image - truth|, bright = wrong. This makes the corrections visible.
    auto errorImage = [](const Image& a, const Image& b) {
        Image d(a.width(), a.height());
        for (std::size_t i = 0; i < d.size(); ++i) d[i] = std::fabs(a[i] - b[i]);
        return d;
    };
    savePNG(errorImage(truth, acq.raw), "5_error_raw.png", 0.0f, 0.3f);
    savePNG(errorImage(truth, pixelsFixed), "6_error_corrected.png", 0.0f, 0.3f);

    std::printf("\nSaved 1_truth.png, 2_raw.png, 3_corrected.png, 4_denoised.png, 5_error_raw.png, 6_error_corrected.png\n");
    return 0;
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "Error: %s\n", ex.what());
    return 1;
  }
}
