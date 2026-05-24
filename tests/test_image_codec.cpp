#include <iostream>
#include <cassert>
#include <cstring>
#include "imgproc/imgproc.hpp"

// 简单测试框架
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

// ============================================================
// ImageBuffer 基础测试
// ============================================================

TEST_CASE(imagebuffer_default) {
    imgproc::ImageBuffer buf;
    ASSERT_EQ(buf.width, 0);
    ASSERT_EQ(buf.height, 0);
    ASSERT_EQ(buf.stride, 0);
    ASSERT_TRUE(buf.empty());
    ASSERT_EQ(buf.dataSize(), 0u);
}

TEST_CASE(imagebuffer_clear) {
    imgproc::ImageBuffer buf;
    buf.width = 100;
    buf.height = 100;
    buf.stride = 300;
    buf.data.resize(30000, 0xAA);
    buf.palette.resize(1024, 0xBB);

    buf.clear();
    ASSERT_EQ(buf.width, 0);
    ASSERT_EQ(buf.height, 0);
    ASSERT_EQ(buf.stride, 0);
    ASSERT_TRUE(buf.empty());
    ASSERT_TRUE(buf.palette.empty());
}

TEST_CASE(imagebuffer_data) {
    imgproc::ImageBuffer buf;
    buf.width = 2;
    buf.height = 2;
    buf.stride = 6;
    buf.data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    ASSERT_FALSE(buf.empty());
    ASSERT_EQ(buf.dataSize(), 12u);
}

// ============================================================
// 图像类型检测测试
// ============================================================

TEST_CASE(detect_jpeg) {
    // JPEG 魔数: FF D8 FF
    uint8_t jpegData[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};
    auto type = imgproc::detectImageType(jpegData, sizeof(jpegData));
    ASSERT_TRUE(type == imgproc::ImageType::JPEG);
}

TEST_CASE(detect_png) {
    // PNG 魔数: 89 50 4E 47 0D 0A 1A 0A
    uint8_t pngData[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    auto type = imgproc::detectImageType(pngData, sizeof(pngData));
    ASSERT_TRUE(type == imgproc::ImageType::PNG);
}

TEST_CASE(detect_bmp) {
    // BMP 魔数: 42 4D
    uint8_t bmpData[] = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
    auto type = imgproc::detectImageType(bmpData, sizeof(bmpData));
    ASSERT_TRUE(type == imgproc::ImageType::BMP);
}

TEST_CASE(detect_unknown) {
    uint8_t unknownData[] = {0x00, 0x01, 0x02, 0x03};
    auto type = imgproc::detectImageType(unknownData, sizeof(unknownData));
    ASSERT_TRUE(type == imgproc::ImageType::Unknown);
}

TEST_CASE(detect_empty) {
    auto type = imgproc::detectImageType(nullptr, 0);
    ASSERT_TRUE(type == imgproc::ImageType::Unknown);
}

TEST_CASE(detect_too_small) {
    uint8_t data[] = {0xFF};
    auto type = imgproc::detectImageType(data, 1);
    ASSERT_TRUE(type == imgproc::ImageType::Unknown);
}

// ============================================================
// 扩展名检测测试
// ============================================================

TEST_CASE(extension_jpeg) {
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("test.jpg") == imgproc::ImageType::JPEG);
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("test.jpeg") == imgproc::ImageType::JPEG);
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("test.JPG") == imgproc::ImageType::JPEG);
}

TEST_CASE(extension_png) {
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("test.png") == imgproc::ImageType::PNG);
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("test.PNG") == imgproc::ImageType::PNG);
}

TEST_CASE(extension_bmp) {
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("test.bmp") == imgproc::ImageType::BMP);
}

TEST_CASE(extension_unknown) {
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("test.txt") == imgproc::ImageType::Unknown);
    ASSERT_TRUE(imgproc::detectImageTypeByExtension("noext") == imgproc::ImageType::Unknown);
}

// ============================================================
// 编解码器工厂测试
// ============================================================

TEST_CASE(cross_codec_creation) {
    auto codec = imgproc::createCrossCodec();
    ASSERT_TRUE(codec != nullptr);
}

#ifdef USE_WINDOWS_API
TEST_CASE(win_codec_creation) {
    auto codec = imgproc::createWinCodec();
    ASSERT_TRUE(codec != nullptr);
}
#endif

// ============================================================
// BMP 内存编解码测试
// ============================================================

TEST_CASE(bmp_roundtrip_24bit) {
    auto codec = imgproc::createCrossCodec();
    ASSERT_TRUE(codec != nullptr);

    // 创建测试图像 (BGR24)
    imgproc::ImageBuffer original;
    original.width = 4;
    original.height = 4;
    original.format = imgproc::PixelFormat::BGR24;
    original.stride = 12; // 4 * 3
    original.data.resize(48);
    for (int i = 0; i < 48; ++i) {
        original.data[i] = static_cast<uint8_t>(i);
    }

    // 保存为 BMP 内存
    std::vector<uint8_t> bmpData;
    ASSERT_TRUE(codec->saveToBmpMemory(original, bmpData));
    ASSERT_GT(bmpData.size(), 54u); // 至少包含文件头和信息头

    // 从 BMP 内存加载
    imgproc::ImageBuffer loaded;
    ASSERT_TRUE(codec->loadFromMemory(bmpData.data(), bmpData.size(),
                                       imgproc::ImageType::BMP, loaded));
    ASSERT_EQ(loaded.width, 4);
    ASSERT_EQ(loaded.height, 4);
    ASSERT_TRUE(loaded.format == imgproc::PixelFormat::BGR24);
    ASSERT_FALSE(loaded.empty());
}

TEST_CASE(bmp_roundtrip_32bit) {
    auto codec = imgproc::createCrossCodec();
    ASSERT_TRUE(codec != nullptr);

    imgproc::ImageBuffer original;
    original.width = 8;
    original.height = 8;
    original.format = imgproc::PixelFormat::BGRA32;
    original.stride = 32; // 8 * 4
    original.data.resize(256);
    for (int i = 0; i < 256; ++i) {
        original.data[i] = static_cast<uint8_t>(i % 256);
    }

    std::vector<uint8_t> bmpData;
    ASSERT_TRUE(codec->saveToBmpMemory(original, bmpData));

    imgproc::ImageBuffer loaded;
    ASSERT_TRUE(codec->loadFromMemory(bmpData.data(), bmpData.size(),
                                       imgproc::ImageType::BMP, loaded));
    ASSERT_EQ(loaded.width, 8);
    ASSERT_EQ(loaded.height, 8);
}

// ============================================================
// JPEG 内存编解码测试
// ============================================================

TEST_CASE(jpeg_roundtrip) {
    auto codec = imgproc::createCrossCodec();
    ASSERT_TRUE(codec != nullptr);

    // 创建测试图像
    imgproc::ImageBuffer original;
    original.width = 64;
    original.height = 64;
    original.format = imgproc::PixelFormat::BGR24;
    original.stride = 192; // 64 * 3
    original.data.resize(64 * 192);
    for (size_t i = 0; i < original.data.size(); ++i) {
        original.data[i] = static_cast<uint8_t>(i % 256);
    }

    // 保存为 JPEG
    std::vector<uint8_t> jpegData;
    ASSERT_TRUE(codec->saveToJpegMemory(original, jpegData, 90));
    ASSERT_GT(jpegData.size(), 0u);

    // JPEG 应该比原始数据小 (有损压缩)
    // 注意: 对于随机数据 JPEG 可能更大, 但对于渐变数据应该更小

    // 从 JPEG 加载
    imgproc::ImageBuffer loaded;
    ASSERT_TRUE(codec->loadFromMemory(jpegData.data(), jpegData.size(),
                                       imgproc::ImageType::JPEG, loaded));
    ASSERT_EQ(loaded.width, 64);
    ASSERT_EQ(loaded.height, 64);
}

// ============================================================
// 主函数
// ============================================================

int main() {
    std::cout << "=== imgproc_lib Image Codec Tests ===" << std::endl;

    // 所有测试通过全局对象构造自动运行
    // 这里不需要额外调用

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "  Passed: " << g_testsPassed << std::endl;
    std::cout << "  Failed: " << g_testsFailed << std::endl;

    return (g_testsFailed > 0) ? 1 : 0;
}
