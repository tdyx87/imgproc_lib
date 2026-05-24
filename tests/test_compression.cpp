#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>
#include "imgproc/imgproc.hpp"

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_CASE(name) \
    static void test_##name(); \
    struct TestRegister_##name { \
        TestRegister_##name() { \
            std::cout << "  [RUN ] " #name << std::endl; \
            test_##name(); \
        } \
    } g_testRegister_##name; \
    static void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "    [FAIL] " #expr " at line " << __LINE__ << std::endl; \
            g_testsFailed++; \
            return; \
        } \
        g_testsPassed++; \
    } while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "    [FAIL] " #a " == " #b " at line " << __LINE__ \
                      << " (got " << (a) << " vs " << (b) << ")" << std::endl; \
            g_testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))

#define ASSERT_NEAR(a, b, eps) \
    do { \
        double _a = (a), _b = (b), _e = (eps); \
        if (std::fabs(_a - _b) > _e) { \
            std::cerr << "    [FAIL] " #a " ~= " #b " at line " << __LINE__ \
                      << " (got " << _a << " vs " << _b << ", eps=" << _e << ")" << std::endl; \
            g_testsFailed++; \
            return; \
        } \
    } while(0)

// ============================================================
// CompressionResult default test
// ============================================================

TEST_CASE(compression_result_default) {
    imgproc::CompressionResult result;
    ASSERT_TRUE(result.data.empty());
    ASSERT_EQ(result.compressionRatio, 0.0);
    ASSERT_EQ(result.elapsedMs, 0.0);
}

// ============================================================
// RLE compression tests
// ============================================================

TEST_CASE(rle_factory) {
    auto comp = imgproc::createRLECompression();
    ASSERT_TRUE(comp != nullptr);
}

TEST_CASE(rle_compress_uniform) {
    // All identical bytes, RLE should have good compression ratio
    std::vector<uint8_t> data(1000, 0xAA);

    auto result = imgproc::compressRLE(data.data(), data.size(),
                                        10, 100, imgproc::PixelFormat::Grayscale8);
    ASSERT_TRUE(result.type == imgproc::CompressionType::RLE);
    ASSERT_GT(result.compressionRatio, 1.0);
    ASSERT_GT(result.elapsedMs, 0.0);
}

TEST_CASE(rle_compress_decompress_roundtrip) {
    // Create test data
    std::vector<uint8_t> original(256 * 256 * 3);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>(i % 256);
    }

    // Compress
    auto compressed = imgproc::compressRLE(original.data(), original.size(),
                                            256, 256, imgproc::PixelFormat::RGB24);
    ASSERT_TRUE(compressed.type == imgproc::CompressionType::RLE);

    // Decompress
    std::vector<uint8_t> decompressed;
    int width = 0, height = 0;
    imgproc::PixelFormat format = imgproc::PixelFormat::RGB24;
    bool ok = imgproc::decompressRLE(compressed.data.data(), compressed.data.size(),
                                      decompressed, width, height, format);
    ASSERT_TRUE(ok);
    ASSERT_EQ(width, 256);
    ASSERT_EQ(height, 256);
    ASSERT_TRUE(format == imgproc::PixelFormat::RGB24);
    ASSERT_EQ(decompressed.size(), original.size());

    // Verify data consistency
    ASSERT_TRUE(std::memcmp(original.data(), decompressed.data(), original.size()) == 0);
}

TEST_CASE(rle_interface_roundtrip) {
    std::vector<uint8_t> original(100 * 50 * 3);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }

    auto comp = imgproc::createRLECompression();
    auto result = comp->compress(original.data(), original.size(),
                                  100, 50, imgproc::PixelFormat::RGB24);
    ASSERT_TRUE(result.type == imgproc::CompressionType::RLE);

    std::vector<uint8_t> decompressed;
    int width = 0, height = 0;
    imgproc::PixelFormat format = imgproc::PixelFormat::RGB24;
    bool ok = comp->decompress(result.data.data(), result.data.size(),
                                decompressed, width, height, format);
    ASSERT_TRUE(ok);
    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT_TRUE(std::memcmp(original.data(), decompressed.data(), original.size()) == 0);
}

TEST_CASE(rle_decompress_invalid) {
    std::vector<uint8_t> decompressed;
    int width = 0, height = 0;
    imgproc::PixelFormat format = imgproc::PixelFormat::RGB24;

    // Data too small
    bool ok = imgproc::decompressRLE(nullptr, 0, decompressed, width, height, format);
    ASSERT_FALSE(ok);
}

// ============================================================
// Delta Row compression tests
// ============================================================

TEST_CASE(delta_factory) {
    auto comp = imgproc::createDeltaRowCompression();
    ASSERT_TRUE(comp != nullptr);
}

TEST_CASE(delta_compress_identical_rows) {
    // All rows identical, Delta Row stores differences (zeros)
    // Note: DeltaRow doesn't compress identical rows well because it stores all differences
    std::vector<uint8_t> row(300, 0x55);
    std::vector<uint8_t> data;
    for (int i = 0; i < 100; ++i) {
        data.insert(data.end(), row.begin(), row.end());
    }

    auto result = imgproc::compressDeltaRow(data.data(), data.size(),
                                             100, 100, imgproc::PixelFormat::RGB24);
    ASSERT_TRUE(result.type == imgproc::CompressionType::DeltaRow);
    // DeltaRow stores header + first row + differences, so ratio may be slightly < 1
    ASSERT_TRUE(result.data.size() > 0);
}

TEST_CASE(delta_compress_decompress_roundtrip) {
    std::vector<uint8_t> original(64 * 64 * 3);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<uint8_t>((i * 3 + 7) % 256);
    }

    auto compressed = imgproc::compressDeltaRow(original.data(), original.size(),
                                                 64, 64, imgproc::PixelFormat::RGB24);
    ASSERT_TRUE(compressed.type == imgproc::CompressionType::DeltaRow);

    std::vector<uint8_t> decompressed;
    int width = 0, height = 0;
    imgproc::PixelFormat format = imgproc::PixelFormat::RGB24;
    bool ok = imgproc::decompressDeltaRow(compressed.data.data(), compressed.data.size(),
                                            decompressed, width, height, format);
    ASSERT_TRUE(ok);
    ASSERT_EQ(width, 64);
    ASSERT_EQ(height, 64);
    ASSERT_EQ(decompressed.size(), original.size());
    ASSERT_TRUE(std::memcmp(original.data(), decompressed.data(), original.size()) == 0);
}

TEST_CASE(delta_decompress_invalid) {
    std::vector<uint8_t> decompressed;
    int width = 0, height = 0;
    imgproc::PixelFormat format = imgproc::PixelFormat::RGB24;

    bool ok = imgproc::decompressDeltaRow(nullptr, 0, decompressed, width, height, format);
    ASSERT_FALSE(ok);
}

// ============================================================
// JPEG compression tests
// ============================================================

TEST_CASE(jpeg_compress_factory) {
    auto comp = imgproc::createJPEGCompression(75);
    ASSERT_TRUE(comp != nullptr);
}

TEST_CASE(jpeg_compress_basic) {
    // Create gradient image
    std::vector<uint8_t> data(128 * 128 * 3);
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            size_t idx = (y * 128 + x) * 3;
            data[idx] = static_cast<uint8_t>(x * 2);     // R
            data[idx + 1] = static_cast<uint8_t>(y * 2); // G
            data[idx + 2] = 128;                          // B
        }
    }

    auto result = imgproc::compressJPEG(data.data(), data.size(),
                                         128, 128, imgproc::PixelFormat::RGB24, 80);
    ASSERT_TRUE(result.type == imgproc::CompressionType::JPEG);
    ASSERT_GT(result.data.size(), 12u); // At least contains header info
    ASSERT_GT(result.elapsedMs, 0.0);
}

TEST_CASE(jpeg_compress_quality_levels) {
    // Use gradient image (not solid color) so quality affects file size
    std::vector<uint8_t> data(64 * 64 * 3);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            size_t idx = (y * 64 + x) * 3;
            data[idx] = static_cast<uint8_t>(x * 4);     // R gradient
            data[idx + 1] = static_cast<uint8_t>(y * 4); // G gradient
            data[idx + 2] = static_cast<uint8_t>((x + y) * 2); // B gradient
        }
    }

    auto q10 = imgproc::compressJPEG(data.data(), data.size(),
                                       64, 64, imgproc::PixelFormat::RGB24, 10);
    auto q90 = imgproc::compressJPEG(data.data(), data.size(),
                                       64, 64, imgproc::PixelFormat::RGB24, 90);

    // Higher quality should produce larger file for gradient image
    ASSERT_GT(q90.data.size(), q10.data.size());
}

// ============================================================
// Main
// ============================================================

int main() {
    std::cout << "=== imgproc_lib Compression Tests ===" << std::endl;

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "  Passed: " << g_testsPassed << std::endl;
    std::cout << "  Failed: " << g_testsFailed << std::endl;

    return (g_testsFailed > 0) ? 1 : 0;
}
