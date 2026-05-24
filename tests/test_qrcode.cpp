#include <iostream>
#include <cassert>
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
// QRCodeResult 默认值测试
// ============================================================

TEST_CASE(qr_result_default) {
    imgproc::QRCodeResult result;
    ASSERT_TRUE(!result.success);
    ASSERT_TRUE(result.text.empty());
    ASSERT_EQ(result.qrVersion, 0);
    ASSERT_EQ(result.errorCorrectionLevel, 0);
    ASSERT_TRUE(result.bitmap1bit.empty());
}

// ============================================================
// QRCodeReader 工厂测试
// ============================================================

TEST_CASE(qr_reader_creation) {
    auto reader = imgproc::createQRCodeReader();
    ASSERT_TRUE(reader != nullptr);
}

// ============================================================
// QRCodeReader 空数据测试
// ============================================================

TEST_CASE(qr_read_empty_memory) {
    auto reader = imgproc::createQRCodeReader();
    ASSERT_TRUE(reader != nullptr);

    auto result = reader->readFromMemory(nullptr, 0);
    ASSERT_TRUE(!result.success);
}

TEST_CASE(qr_read_invalid_data) {
    auto reader = imgproc::createQRCodeReader();
    ASSERT_TRUE(reader != nullptr);

    uint8_t invalidData[] = {0x00, 0x01, 0x02, 0x03};
    auto result = reader->readFromMemory(invalidData, sizeof(invalidData));
    ASSERT_TRUE(!result.success);
}

TEST_CASE(qr_read_nonexistent_file) {
    auto reader = imgproc::createQRCodeReader();
    ASSERT_TRUE(reader != nullptr);

    auto result = reader->readFromFile("nonexistent_qrcode_file.png");
    ASSERT_TRUE(!result.success);
}

// ============================================================
// QRCodeReader 多码读取测试
// ============================================================

TEST_CASE(qr_read_multiple_nonexistent) {
    auto reader = imgproc::createQRCodeReader();
    ASSERT_TRUE(reader != nullptr);

    auto results = reader->readMultipleFromFile("nonexistent_qrcode_file.png");
    ASSERT_TRUE(results.empty());
}

// ============================================================
// 便捷函数测试
// ============================================================

TEST_CASE(qr_convenience_read_from_memory) {
    uint8_t invalidData[] = {0x00, 0x01, 0x02};
    auto result = imgproc::readQRCodeFromMemory(invalidData, sizeof(invalidData));
    ASSERT_TRUE(!result.success);
}

TEST_CASE(qr_convenience_read_from_file) {
    auto result = imgproc::readQRCode("nonexistent_file.png");
    ASSERT_TRUE(!result.success);
}

// ============================================================
// 主函数
// ============================================================

int main() {
    std::cout << "=== imgproc_lib QRCode Reader Tests ===" << std::endl;

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "  Passed: " << g_testsPassed << std::endl;
    std::cout << "  Failed: " << g_testsFailed << std::endl;

    return (g_testsFailed > 0) ? 1 : 0;
}
