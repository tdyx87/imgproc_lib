#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <cstring>
#include <cmath>
#include <sstream>
#include <functional>
#include "imgproc/imgproc.hpp"

using namespace std::chrono;

class Timer {
public:
    void start() { start_ = steady_clock::now(); }
    double elapsedMs() const {
        return duration_cast<microseconds>(steady_clock::now() - start_).count() / 1000.0;
    }
private:
    steady_clock::time_point start_;
};

struct BenchResult {
    std::string name;
    double avgMs;
    double minMs;
    double maxMs;
    size_t outputSize;
    double ratio;
};

static BenchResult runBench(const std::string& name, int iterations, std::function<void()> fn) {
    BenchResult result;
    result.name = name;

    // 预热
    fn();

    double total = 0;
    double minT = 1e9, maxT = 0;
    for (int i = 0; i < iterations; ++i) {
        Timer t;
        t.start();
        fn();
        double elapsed = t.elapsedMs();
        total += elapsed;
        if (elapsed < minT) minT = elapsed;
        if (elapsed > maxT) maxT = elapsed;
    }
    result.avgMs = total / iterations;
    result.minMs = minT;
    result.maxMs = maxT;
    return result;
}

// 生成测试图像数据
static imgproc::ImageBuffer createTestImage(int size, imgproc::PixelFormat fmt) {
    imgproc::ImageBuffer img;
    img.width = size;
    img.height = size;
    img.format = fmt;

    int bpp = 3;
    if (fmt == imgproc::PixelFormat::RGBA32 || fmt == imgproc::PixelFormat::BGRA32) bpp = 4;
    if (fmt == imgproc::PixelFormat::Grayscale8) bpp = 1;

    img.stride = size * bpp;
    img.data.resize(static_cast<size_t>(img.stride) * size);

    // 渐变 + 一些纹理
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            size_t idx = (y * img.stride) + x * bpp;
            uint8_t val = static_cast<uint8_t>((x * 255 / size + y * 255 / size) / 2);
            switch (fmt) {
                case imgproc::PixelFormat::BGR24:
                    img.data[idx] = val;
                    img.data[idx+1] = static_cast<uint8_t>(255 - val);
                    img.data[idx+2] = 128;
                    break;
                case imgproc::PixelFormat::RGB24:
                    img.data[idx] = 128;
                    img.data[idx+1] = static_cast<uint8_t>(255 - val);
                    img.data[idx+2] = val;
                    break;
                case imgproc::PixelFormat::BGRA32:
                    img.data[idx] = val;
                    img.data[idx+1] = static_cast<uint8_t>(255 - val);
                    img.data[idx+2] = 128;
                    img.data[idx+3] = 255;
                    break;
                case imgproc::PixelFormat::Grayscale8:
                    img.data[idx] = val;
                    break;
                default:
                    img.data[idx] = val;
                    img.data[idx+1] = static_cast<uint8_t>(255 - val);
                    img.data[idx+2] = 128;
                    break;
            }
        }
    }
    return img;
}

// 生成纯色图像 (用于测试最佳压缩)
static imgproc::ImageBuffer createUniformImage(int size, imgproc::PixelFormat fmt) {
    imgproc::ImageBuffer img;
    img.width = size;
    img.height = size;
    img.format = fmt;
    int bpp = 3;
    img.stride = size * bpp;
    img.data.resize(static_cast<size_t>(img.stride) * size, 128);
    return img;
}

static void printTableHeader(const std::string& title) {
    std::cout << "\n" << title << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::left << std::setw(35) << "Test"
              << std::right << std::setw(10) << "Avg(ms)"
              << std::setw(10) << "Min(ms)"
              << std::setw(10) << "Max(ms)"
              << std::setw(12) << "Output"
              << std::setw(10) << "Ratio"
              << std::endl;
    std::cout << std::string(80, '-') << std::endl;
}

static void printResult(const BenchResult& r) {
    std::cout << std::left << std::setw(35) << r.name
              << std::right << std::fixed << std::setprecision(3)
              << std::setw(10) << r.avgMs
              << std::setw(10) << r.minMs
              << std::setw(10) << r.maxMs
              << std::setw(10) << r.outputSize
              << std::setw(10) << std::setprecision(2) << r.ratio
              << std::endl;
}

// ============================================================
// 1. 图像编解码对比: Windows API vs 跨平台
// ============================================================

static void benchmarkCodecComparison() {
    printTableHeader("=== 1. Image Encode/Decode: Windows API vs Cross-Platform ===");

    const int sizes[] = {64, 256, 1024};
    const int iterations = 20;

    for (int size : sizes) {
        std::cout << "\n--- Image size: " << size << "x" << size << " ---" << std::endl;

        auto img = createTestImage(size, imgproc::PixelFormat::BGR24);
        size_t rawSize = img.data.size();

#ifdef USE_WINDOWS_API
        // Windows API - JPEG 编码
        {
            auto winCodec = imgproc::createWinCodec();
            std::vector<uint8_t> jpegOut;
            auto r = runBench("Win JPEG Encode q=85", iterations, [&]() {
                winCodec->saveToJpegMemory(img, jpegOut, 85);
            });
            r.outputSize = jpegOut.size();
            r.ratio = static_cast<double>(rawSize) / jpegOut.size();
            printResult(r);
        }

        // Windows API - BMP 编码
        {
            auto winCodec = imgproc::createWinCodec();
            std::vector<uint8_t> bmpOut;
            auto r = runBench("Win BMP Encode", iterations, [&]() {
                winCodec->saveToBmpMemory(img, bmpOut);
            });
            r.outputSize = bmpOut.size();
            r.ratio = static_cast<double>(rawSize) / bmpOut.size();
            printResult(r);
        }
#endif

        // 跨平台 - JPEG 编码
        {
            auto crossCodec = imgproc::createCrossCodec();
            std::vector<uint8_t> jpegOut;
            auto r = runBench("Cross JPEG Encode q=85", iterations, [&]() {
                crossCodec->saveToJpegMemory(img, jpegOut, 85);
            });
            r.outputSize = jpegOut.size();
            r.ratio = static_cast<double>(rawSize) / jpegOut.size();
            printResult(r);
        }

        // 跨平台 - BMP 编码
        {
            auto crossCodec = imgproc::createCrossCodec();
            std::vector<uint8_t> bmpOut;
            auto r = runBench("Cross BMP Encode", iterations, [&]() {
                crossCodec->saveToBmpMemory(img, bmpOut);
            });
            r.outputSize = bmpOut.size();
            r.ratio = static_cast<double>(rawSize) / bmpOut.size();
            printResult(r);
        }

        // JPEG 解码对比
        {
            auto crossCodec = imgproc::createCrossCodec();
            std::vector<uint8_t> jpegData;
            crossCodec->saveToJpegMemory(img, jpegData, 85);

#ifdef USE_WINDOWS_API
            auto winCodec = imgproc::createWinCodec();
            imgproc::ImageBuffer decoded;
            auto r = runBench("Win JPEG Decode", iterations, [&]() {
                winCodec->loadFromMemory(jpegData.data(), jpegData.size(), imgproc::ImageType::JPEG, decoded);
            });
            printResult(r);
#endif

            {
                imgproc::ImageBuffer decoded;
                auto r = runBench("Cross JPEG Decode", iterations, [&]() {
                    crossCodec->loadFromMemory(jpegData.data(), jpegData.size(), imgproc::ImageType::JPEG, decoded);
                });
                printResult(r);
            }
        }
    }
}

// ============================================================
// 2. 压缩算法对比
// ============================================================

static void benchmarkCompression() {
    printTableHeader("=== 2. Compression Algorithm Comparison ===");

    const int sizes[] = {64, 256, 1024};
    const int iterations = 20;

    for (int size : sizes) {
        std::cout << "\n--- Image size: " << size << "x" << size << " (gradient data) ---" << std::endl;

        auto img = createTestImage(size, imgproc::PixelFormat::RGB24);
        size_t rawSize = img.data.size();

        // RLE
        {
            auto r = runBench("RLE Compress", iterations, [&]() {
                imgproc::compressRLE(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24);
            });
            auto result = imgproc::compressRLE(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24);
            r.outputSize = result.data.size();
            r.ratio = static_cast<double>(rawSize) / result.data.size();
            printResult(r);
        }

        // DeltaRow
        {
            auto r = runBench("DeltaRow Compress", iterations, [&]() {
                imgproc::compressDeltaRow(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24);
            });
            auto result = imgproc::compressDeltaRow(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24);
            r.outputSize = result.data.size();
            r.ratio = static_cast<double>(rawSize) / result.data.size();
            printResult(r);
        }

        // JPEG q=50
        {
            auto r = runBench("JPEG Compress q=50", iterations, [&]() {
                imgproc::compressJPEG(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24, 50);
            });
            auto result = imgproc::compressJPEG(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24, 50);
            r.outputSize = result.data.size();
            r.ratio = static_cast<double>(rawSize) / result.data.size();
            printResult(r);
        }

        // JPEG q=75
        {
            auto r = runBench("JPEG Compress q=75", iterations, [&]() {
                imgproc::compressJPEG(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24, 75);
            });
            auto result = imgproc::compressJPEG(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24, 75);
            r.outputSize = result.data.size();
            r.ratio = static_cast<double>(rawSize) / result.data.size();
            printResult(r);
        }

        // JPEG q=90
        {
            auto r = runBench("JPEG Compress q=90", iterations, [&]() {
                imgproc::compressJPEG(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24, 90);
            });
            auto result = imgproc::compressJPEG(img.data.data(), img.data.size(), size, size, imgproc::PixelFormat::RGB24, 90);
            r.outputSize = result.data.size();
            r.ratio = static_cast<double>(rawSize) / result.data.size();
            printResult(r);
        }
    }

    // 特殊场景: 纯色图像
    std::cout << "\n--- Special: Uniform color 256x256 ---" << std::endl;
    {
        auto img = createUniformImage(256, imgproc::PixelFormat::RGB24);
        size_t rawSize = img.data.size();

        auto rle = imgproc::compressRLE(img.data.data(), img.data.size(), 256, 256, imgproc::PixelFormat::RGB24);
        auto delta = imgproc::compressDeltaRow(img.data.data(), img.data.size(), 256, 256, imgproc::PixelFormat::RGB24);
        auto jpeg = imgproc::compressJPEG(img.data.data(), img.data.size(), 256, 256, imgproc::PixelFormat::RGB24, 75);

        BenchResult r;
        r.name = "RLE (uniform)"; r.avgMs = rle.elapsedMs; r.outputSize = rle.data.size(); r.ratio = static_cast<double>(rawSize)/rle.data.size(); printResult(r);
        r.name = "DeltaRow (uniform)"; r.avgMs = delta.elapsedMs; r.outputSize = delta.data.size(); r.ratio = static_cast<double>(rawSize)/delta.data.size(); printResult(r);
        r.name = "JPEG q=75 (uniform)"; r.avgMs = jpeg.elapsedMs; r.outputSize = jpeg.data.size(); r.ratio = static_cast<double>(rawSize)/jpeg.data.size(); printResult(r);
    }
}

// ============================================================
// 3. 文本渲染对比
// ============================================================

static void benchmarkTextRendering() {
    printTableHeader("=== 3. Text Rendering: Windows API vs Cross-Platform ===");

    const int iterations = 50;
    const char* texts[] = {
        "Hello",
        "The quick brown fox jumps over the lazy dog. 0123456789",
        "图像处理库测试 - Image Processing Library Test"
    };
    const char* labels[] = {
        "Short text (5 chars)",
        "Medium text (58 chars)",
        "Mixed CJK+ASCII (45 chars)"
    };

    for (int i = 0; i < 3; ++i) {
        std::cout << "\n--- " << labels[i] << " ---" << std::endl;

        imgproc::TextRenderOptions opts;
        opts.text = texts[i];
        opts.fontSize = 24;

#ifdef USE_WINDOWS_API
        {
            auto renderer = imgproc::createWinTextRenderer();
            imgproc::ImageBuffer result;
            auto r = runBench("Win GDI+ Render", iterations, [&]() {
                renderer->renderToMemory(opts, result);
            });
            r.outputSize = result.dataSize();
            printResult(r);
        }
#endif

        {
            auto renderer = imgproc::createCrossTextRenderer();
            imgproc::ImageBuffer result;
            auto r = runBench("Cross FreeType Render", iterations, [&]() {
                renderer->renderToMemory(opts, result);
            });
            r.outputSize = result.dataSize();
            printResult(r);
        }
    }
}

// ============================================================
// 4. 不同 JPEG 质量级别对比
// ============================================================

static void benchmarkJPEGQuality() {
    printTableHeader("=== 4. JPEG Quality vs Size/Speed Trade-off (256x256) ===");

    auto img = createTestImage(256, imgproc::PixelFormat::RGB24);
    size_t rawSize = img.data.size();
    const int iterations = 20;

    for (int q = 10; q <= 100; q += 10) {
        auto r = runBench("JPEG q=" + std::to_string(q), iterations, [&]() {
            imgproc::compressJPEG(img.data.data(), img.data.size(), 256, 256, imgproc::PixelFormat::RGB24, q);
        });
        auto result = imgproc::compressJPEG(img.data.data(), img.data.size(), 256, 256, imgproc::PixelFormat::RGB24, q);
        r.outputSize = result.data.size();
        r.ratio = static_cast<double>(rawSize) / result.data.size();
        printResult(r);
    }
}

// ============================================================
// 5. 压缩算法适用场景分析
// ============================================================

static void analyzeCompressionScenarios() {
    std::cout << "\n=== 5. Compression Scenario Analysis ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    struct Scenario {
        std::string name;
        int size;
        imgproc::PixelFormat fmt;
        bool uniform;
    };

    Scenario scenarios[] = {
        {"Solid color block", 256, imgproc::PixelFormat::RGB24, true},
        {"Gradient image", 256, imgproc::PixelFormat::RGB24, false},
        {"Photo-like (random)", 256, imgproc::PixelFormat::RGB24, false},
        {"Grayscale text-like", 256, imgproc::PixelFormat::Grayscale8, false},
    };

    for (auto& sc : scenarios) {
        std::cout << "\nScenario: " << sc.name << " (" << sc.size << "x" << sc.size << ")" << std::endl;

        imgproc::ImageBuffer img;
        img.width = sc.size;
        img.height = sc.size;
        img.format = sc.fmt;
        int bpp = (sc.fmt == imgproc::PixelFormat::Grayscale8) ? 1 : 3;
        img.stride = sc.size * bpp;
        img.data.resize(static_cast<size_t>(img.stride) * sc.size);

        if (sc.uniform) {
            std::fill(img.data.begin(), img.data.end(), 128);
        } else if (sc.name.find("random") != std::string::npos) {
            for (size_t i = 0; i < img.data.size(); ++i) {
                img.data[i] = static_cast<uint8_t>((i * 7 + 13 + i * i) % 256);
            }
        } else {
            for (int y = 0; y < sc.size; ++y) {
                for (int x = 0; x < sc.size; ++x) {
                    size_t idx = (y * img.stride) + x * bpp;
                    img.data[idx] = static_cast<uint8_t>((x + y) % 256);
                    if (bpp > 1) { img.data[idx+1] = static_cast<uint8_t>((x * 2) % 256); img.data[idx+2] = static_cast<uint8_t>((y * 2) % 256); }
                }
            }
        }

        size_t rawSize = img.data.size();

        auto rle = imgproc::compressRLE(img.data.data(), img.data.size(), sc.size, sc.size, sc.fmt);
        auto delta = imgproc::compressDeltaRow(img.data.data(), img.data.size(), sc.size, sc.size, sc.fmt);
        auto jpeg50 = imgproc::compressJPEG(img.data.data(), img.data.size(), sc.size, sc.size, sc.fmt, 50);
        auto jpeg75 = imgproc::compressJPEG(img.data.data(), img.data.size(), sc.size, sc.size, sc.fmt, 75);

        std::cout << "  Raw: " << rawSize << " bytes" << std::endl;
        std::cout << "  RLE:      " << std::setw(8) << rle.data.size() << " bytes  ratio=" << std::fixed << std::setprecision(2) << static_cast<double>(rawSize)/rle.data.size() << "  time=" << rle.elapsedMs << "ms" << std::endl;
        std::cout << "  DeltaRow: " << std::setw(8) << delta.data.size() << " bytes  ratio=" << std::fixed << std::setprecision(2) << static_cast<double>(rawSize)/delta.data.size() << "  time=" << delta.elapsedMs << "ms" << std::endl;
        std::cout << "  JPEG q50: " << std::setw(8) << jpeg50.data.size() << " bytes  ratio=" << std::fixed << std::setprecision(2) << static_cast<double>(rawSize)/jpeg50.data.size() << "  time=" << jpeg50.elapsedMs << "ms  (lossy)" << std::endl;
        std::cout << "  JPEG q75: " << std::setw(8) << jpeg75.data.size() << " bytes  ratio=" << std::fixed << std::setprecision(2) << static_cast<double>(rawSize)/jpeg75.data.size() << "  time=" << jpeg75.elapsedMs << "ms  (lossy)" << std::endl;

        // 推荐
        std::string winner;
        if (sc.uniform) {
            winner = "RLE (best for uniform data)";
        } else if (sc.name.find("random") != std::string::npos) {
            winner = "JPEG (best for complex/photographic data)";
        } else {
            winner = "DeltaRow (good for gradual changes) or JPEG";
        }
        std::cout << "  >> Recommendation: " << winner << std::endl;
    }
}

// ============================================================
// 6. Windows API vs 跨平台总结
// ============================================================

static void printComparisonSummary() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "=== COMPARISON SUMMARY ===" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    std::cout << R"(
+---------------------+--------------------------+--------------------------+
| Feature             | Windows API (GDI+/WIC)   | Cross-Platform           |
+---------------------+--------------------------+--------------------------+
| Platform            | Windows only             | All platforms            |
| XP Support          | Yes (GDI+ 1.0)           | Yes                      |
| JPEG Encode Speed   | Medium                   | Fast (libjpeg-turbo)     |
| JPEG Decode Speed   | Medium                   | Fast (libjpeg-turbo)     |
| PNG Support         | Limited (GDI+)           | Full (libpng)            |
| BMP 1-bit Support   | Good                     | Full                     |
| Text Rendering      | Good (system fonts)      | Good (FreeType)          |
| CJK Support         | Native                   | Requires font files      |
| Binary Size Impact  | System DLLs              | Static linking (~2-5MB)  |
| Memory Usage        | Lower (system caches)    | Higher                   |
| Quality Control     | Limited                  | Full control             |
| Dependency          | Windows SDK              | libpng, libjpeg, freetype|
| License             | System license           | Various open source      |
+---------------------+--------------------------+--------------------------+

When to use Windows API:
  - Targeting Windows-only applications
  - Want minimal binary size (no extra libs)
  - Need system font support without bundling fonts
  - Quick prototyping on Windows

When to use Cross-Platform:
  - Need to support multiple platforms
  - Need maximum performance (libjpeg-turbo SIMD)
  - Need fine-grained control over encoding parameters
  - Building a library/SDK for distribution
  - Need PNG read support with full feature set

Compression Algorithm Recommendations:
  - RLE: Best for images with large runs of identical bytes (solid blocks,
    simple graphics, 1-bit images). Very fast encode/decode.
  - DeltaRow: Best for images where adjacent rows are similar (gradients,
    scanned documents, screenshots). Lossless.
  - JPEG: Best for photographic/complex images. Lossy but high compression.
    Use q=50-75 for good balance, q=85-95 for high quality.
)" << std::endl;
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "  imgproc_lib - Comprehensive Benchmark Suite" << std::endl;
    std::cout << "========================================================" << std::endl;

#ifdef USE_WINDOWS_API
    std::cout << "[INFO] Windows API (GDI+) enabled" << std::endl;
#else
    std::cout << "[INFO] Windows API (GDI+) disabled - cross-platform only" << std::endl;
#endif

    benchmarkCodecComparison();
    benchmarkCompression();
    benchmarkTextRendering();
    benchmarkJPEGQuality();
    analyzeCompressionScenarios();
    printComparisonSummary();

    std::cout << "\n=== Benchmark Complete ===" << std::endl;
    return 0;
}
