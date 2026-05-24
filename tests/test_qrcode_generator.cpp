#include <iostream>
#include <cassert>
#include "imgproc/imgproc.hpp"

#ifndef TEST_OUTPUT_DIR
#define TEST_OUTPUT_DIR "f:/project/product/test/imgproc_lib/test_output"
#endif

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_CASE(name) \
    static void test_##name(); \
    struct TestRunner_##name { TestRunner_##name() { test_##name(); } } runner_##name; \
    static void test_##name()

#define ASSERT_TRUE(x) do { if (!(x)) { std::cerr << "  FAIL: " #x << " (line " << __LINE__ << ")" << std::endl; g_testsFailed++; return; } } while(0)
#define ASSERT_FALSE(x) ASSERT_TRUE(!(x))
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "  FAIL: " #a " == " #b << " (line " << __LINE__ << ")" << std::endl; g_testsFailed++; return; } } while(0)
#define ASSERT_GT(a, b) do { if (!((a) > (b))) { std::cerr << "  FAIL: " #a " > " #b << " (line " << __LINE__ << ")" << std::endl; g_testsFailed++; return; } } while(0)

// ============================================================
// 二维码生成测试
// ============================================================

TEST_CASE(qrgen_factory) {
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);
    g_testsPassed++;
}

TEST_CASE(qrgen_basic) {
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);

    imgproc::QRCodeGenerateOptions opts;
    opts.text = "Hello World";
    opts.width = 256;
    opts.height = 256;

    imgproc::ImageBuffer result;
    bool ok = gen->generate(opts, result);
    ASSERT_TRUE(ok);
    ASSERT_GT(result.width, 0);
    ASSERT_GT(result.height, 0);
    // 二维码尺寸由模块数和缩放因子决定，不一定精确匹配请求的 256x256
    ASSERT_GT(result.width, 200);
    ASSERT_GT(result.height, 200);
    ASSERT_EQ(result.format, imgproc::PixelFormat::BGRA32);

    auto codec = imgproc::createCrossCodec();
    if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/qrgen_basic.bmp");
    g_testsPassed++;
}

TEST_CASE(qrgen_chinese) {
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);

    imgproc::QRCodeGenerateOptions opts;
    opts.text = "你好世界";
    opts.width = 256;
    opts.height = 256;

    imgproc::ImageBuffer result;
    bool ok = gen->generate(opts, result);
    ASSERT_TRUE(ok);
    ASSERT_GT(result.width, 0);
    ASSERT_GT(result.height, 0);

    auto codec = imgproc::createCrossCodec();
    if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/qrgen_chinese.bmp");
    g_testsPassed++;
}

TEST_CASE(qrgen_url) {
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);

    imgproc::QRCodeGenerateOptions opts;
    opts.text = "https://www.example.com";
    opts.width = 300;
    opts.height = 300;
    opts.eccLevel = 2; // Q

    imgproc::ImageBuffer result;
    bool ok = gen->generate(opts, result);
    ASSERT_TRUE(ok);
    ASSERT_GT(result.width, 250);
    ASSERT_GT(result.height, 250);

    auto codec = imgproc::createCrossCodec();
    if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/qrgen_url.bmp");
    g_testsPassed++;
}

TEST_CASE(qrgen_ecc_levels) {
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);

    const char* levels[] = {"L", "M", "Q", "H"};
    for (int ecc = 0; ecc < 4; ++ecc) {
        imgproc::QRCodeGenerateOptions opts;
        opts.text = "Test ECC";
        opts.width = 128;
        opts.height = 128;
        opts.eccLevel = ecc;

        imgproc::ImageBuffer result;
        bool ok = gen->generate(opts, result);
        ASSERT_TRUE(ok);
        ASSERT_GT(result.width, 0);

        auto codec = imgproc::createCrossCodec();
        if (codec) {
            char path[256];
            snprintf(path, sizeof(path), TEST_OUTPUT_DIR "/qrgen_ecc_%s.bmp", levels[ecc]);
            codec->saveToBmpFile(result, path);
        }
    }
    g_testsPassed++;
}

TEST_CASE(qrgen_custom_colors) {
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);

    imgproc::QRCodeGenerateOptions opts;
    opts.text = "Color Test";
    opts.width = 200;
    opts.height = 200;
    opts.fgColor = 0x0000FF; // 蓝色前景
    opts.bgColor = 0xFFFF00; // 黄色背景

    imgproc::ImageBuffer result;
    bool ok = gen->generate(opts, result);
    ASSERT_TRUE(ok);
    ASSERT_GT(result.width, 0);

    auto codec = imgproc::createCrossCodec();
    if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/qrgen_colors.bmp");
    g_testsPassed++;
}

TEST_CASE(qrgen_large_text) {
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);

    imgproc::QRCodeGenerateOptions opts;
    opts.text = "This is a longer text that requires more QR code modules to encode. "
                "The QR code should automatically adjust its version to fit the data.";
    opts.width = 400;
    opts.height = 400;
    opts.eccLevel = 1;

    imgproc::ImageBuffer result;
    bool ok = gen->generate(opts, result);
    ASSERT_TRUE(ok);
    ASSERT_EQ(result.width, 400);

    auto codec = imgproc::createCrossCodec();
    if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/qrgen_large.bmp");
    g_testsPassed++;
}

TEST_CASE(qrgen_roundtrip) {
    // 生成二维码 -> 保存 -> 验证文件存在
    auto gen = imgproc::createQRCodeGenerator();
    ASSERT_TRUE(gen != nullptr);

    std::string testText = "RoundTrip Test 12345";

    imgproc::QRCodeGenerateOptions opts;
    opts.text = testText;
    opts.width = 256;
    opts.height = 256;
    opts.eccLevel = 1;

    // 生成到内存
    imgproc::ImageBuffer result;
    bool ok = gen->generate(opts, result);
    ASSERT_TRUE(ok);
    ASSERT_GT(result.width, 0);
    ASSERT_GT(result.height, 0);

    // 保存为 BMP
    std::string path = TEST_OUTPUT_DIR "/qrgen_roundtrip.bmp";
    ok = gen->generateToFile(opts, path);
    ASSERT_TRUE(ok);

    // 验证 ImageBuffer 数据非空
    bool hasBlack = false;
    for (size_t i = 0; i < result.data.size(); i += 4) {
        if (result.data[i] < 128) { hasBlack = true; break; }
    }
    ASSERT_TRUE(hasBlack);

    g_testsPassed++;
}

TEST_CASE(qrgen_convenience) {
    imgproc::QRCodeGenerateOptions opts;
    opts.text = "Convenience";
    opts.width = 128;
    opts.height = 128;

    imgproc::ImageBuffer result;
    bool ok = imgproc::generateQRCode(opts, result);
    ASSERT_TRUE(ok);
    ASSERT_GT(result.width, 0);
    g_testsPassed++;
}

int main() {
    std::cout << "Running QR Code Generator tests..." << std::endl;
    std::cout << "=== Results ===" << std::endl;
    std::cout << "  Passed: " << g_testsPassed << std::endl;
    std::cout << "  Failed: " << g_testsFailed << std::endl;
    return g_testsFailed > 0 ? 1 : 0;
}
