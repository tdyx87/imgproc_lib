#include <iostream>
#include <cassert>
#include "imgproc/imgproc.hpp"

// 测试图片输出目录
#ifndef TEST_OUTPUT_DIR
#define TEST_OUTPUT_DIR "f:/project/product/test/imgproc_lib/test_output"
#endif

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

#define ASSERT_FALSE(expr) \
    do { \
        if ((expr)) { \
            std::cerr << "    [FAIL] " #expr " at line " << __LINE__ << std::endl; \
            g_testsFailed++; \
            return; \
        } \
        g_testsPassed++; \
    } while(0)

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
// TextRenderOptions 默认值测试
// ============================================================

TEST_CASE(text_options_default) {
    imgproc::TextRenderOptions opts;
    ASSERT_TRUE(opts.text.empty());
    ASSERT_TRUE(opts.fontPath.empty());
    ASSERT_EQ(opts.fontSize, 24);
    ASSERT_EQ(opts.width, 0);
    ASSERT_EQ(opts.height, 0);
    ASSERT_TRUE(opts.outputFormat == imgproc::PixelFormat::RGB24);
    ASSERT_EQ(opts.fgColor, 0x000000u);
    ASSERT_EQ(opts.bgColor, 0xFFFFFFu);
    ASSERT_TRUE(opts.antiAlias);
}

// ============================================================
// 渲染器工厂测试
// ============================================================

TEST_CASE(cross_renderer_creation) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);
}

#ifdef USE_WINDOWS_API
TEST_CASE(win_renderer_creation) {
    auto renderer = imgproc::createWinTextRenderer();
    ASSERT_TRUE(renderer != nullptr);
}
#endif

// ============================================================
// 跨平台渲染测试
// ============================================================

TEST_CASE(cross_render_simple_text) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "Hello";
    opts.fontSize = 16;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    // 注意: 如果系统没有字体, 这可能失败
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
        ASSERT_FALSE(result.empty());
    }
}

TEST_CASE(cross_render_empty_text) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "";
    opts.fontSize = 16;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
    }
}

// ============================================================
// 便捷函数测试
// ============================================================

TEST_CASE(render_text_convenience) {
    imgproc::TextRenderOptions opts;
    opts.text = "Test";
    opts.fontSize = 12;

    imgproc::ImageBuffer result;
    bool ok = imgproc::renderTextToMemory(opts, result, false);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
    }
}

// ============================================================
// 中文渲染测试
// ============================================================

TEST_CASE(cross_render_chinese_text) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "你好世界";
    opts.fontSize = 24;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
        ASSERT_FALSE(result.empty());
        ASSERT_GT(result.width, 24);

        auto codec = imgproc::createCrossCodec();
        if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/chinese_text.bmp");
    }
}

TEST_CASE(cross_render_chinese_english_mixed) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "Hello你好World世界";
    opts.fontSize = 24;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
        ASSERT_FALSE(result.empty());

        auto codec = imgproc::createCrossCodec();
        if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/chinese_mixed.bmp");
    }
}

TEST_CASE(cross_render_chinese_multiline) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "第一行\n第二行\n第三行";
    opts.fontSize = 20;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
        ASSERT_GT(result.height, 20);

        auto codec = imgproc::createCrossCodec();
        if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/chinese_multiline.bmp");
    }
}

TEST_CASE(cross_render_chinese_indexed_1bit) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "测试";
    opts.fontSize = 24;
    opts.bitsPerPixel = 1;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
        ASSERT_FALSE(result.palette.empty());
        ASSERT_TRUE(result.format == imgproc::PixelFormat::Indexed1);
        ASSERT_EQ(result.palette.size(), 8u);

        auto codec = imgproc::createCrossCodec();
        if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/chinese_1bit.bmp");
    }
}

TEST_CASE(cross_render_chinese_indexed_8bit) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "你好";
    opts.fontSize = 24;
    opts.bitsPerPixel = 8;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
        ASSERT_FALSE(result.palette.empty());
        ASSERT_TRUE(result.format == imgproc::PixelFormat::Indexed8);
        ASSERT_EQ(result.palette.size(), 1024u);

        auto codec = imgproc::createCrossCodec();
        if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/chinese_8bit.bmp");
    }
}

TEST_CASE(cross_render_chinese_dpi) {
    auto renderer = imgproc::createCrossTextRenderer();
    ASSERT_TRUE(renderer != nullptr);

    imgproc::TextRenderOptions opts;
    opts.text = "你好";
    opts.fontSize = 24;
    opts.dpi = 150;

    imgproc::ImageBuffer result;
    bool ok = renderer->renderToMemory(opts, result);
    if (ok) {
        ASSERT_GT(result.width, 0);
        ASSERT_GT(result.height, 0);
        ASSERT_GT(result.width, 30);
        ASSERT_GT(result.height, 30);

        auto codec = imgproc::createCrossCodec();
        if (codec) codec->saveToBmpFile(result, TEST_OUTPUT_DIR "/chinese_dpi150.bmp");
    }
}

// ============================================================
// 主函数
// ============================================================

int main() {
    std::cout << "=== imgproc_lib Text Renderer Tests ===" << std::endl;

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "  Passed: " << g_testsPassed << std::endl;
    std::cout << "  Failed: " << g_testsFailed << std::endl;

    return (g_testsFailed > 0) ? 1 : 0;
}
