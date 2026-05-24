/**
 * 条形码生成与读取单元测试
 * 测试 zint 生成 → zxing 读取 的完整流程
 */

#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>
#include "imgproc/barcode_generator.hpp"
#include "imgproc/qrcode_generator.hpp"
#include "imgproc/qrcode_reader.hpp"

static int passed = 0;
static int failed = 0;

#define TEST(name) \
    do { std::cout << "[RUN ] " << name << std::endl; } while(0)

#define PASS(name) \
    do { std::cout << "[PASS] " << name << std::endl; passed++; } while(0)

#define FAIL(name, msg) \
    do { std::cout << "[FAIL] " << name << ": " << msg << std::endl; failed++; } while(0)

#define CHECK(cond, name, msg) \
    do { if (cond) { PASS(name); } else { FAIL(name, msg); } } while(0)

// 辅助：删除文件
static void removeFile(const char* path) {
    std::remove(path);
}

// ============================================================
// 一维码生成测试
// ============================================================

static void test_code128() {
    const char* name = "Code128 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "Hello-2024";
    opts.type = imgproc::BarcodeType::Code128;
    opts.width = 300;
    opts.height = 100;

    // 生成
    bool genOk = imgproc::generateBarcodeToFile(opts, "test_code128.png");
    if (!genOk) {
        // 尝试直接生成 ImageBuffer 看看哪一步失败
        imgproc::ImageBuffer img;
        bool genOk2 = imgproc::generateBarcode(opts, img);
        if (genOk2) {
            std::string msg = "生成 ImageBuffer 成功但保存失败: " + std::to_string(img.width) + "x" + std::to_string(img.height);
            FAIL(name, msg.c_str());
        } else {
            FAIL(name, "生成失败");
        }
        return;
    }

    // 读取
    auto result = imgproc::readBarcode("test_code128.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "Hello-2024", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_code128.png");
}

static void test_code39() {
    const char* name = "Code39 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "CODE39-TEST";
    opts.type = imgproc::BarcodeType::Code39;
    opts.width = 300;
    opts.height = 100;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_code39.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_code39.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "CODE39-TEST", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_code39.png");
}

static void test_ean13() {
    const char* name = "EAN-13 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "6901234567892";
    opts.type = imgproc::BarcodeType::EAN13;
    opts.width = 300;
    opts.height = 100;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_ean13.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_ean13.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "6901234567892", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_ean13.png");
}

static void test_ean8() {
    const char* name = "EAN-8 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "96385074";
    opts.type = imgproc::BarcodeType::EAN8;
    opts.width = 200;
    opts.height = 100;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_ean8.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_ean8.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        // zxing 可能返回带校验位的完整码
        CHECK(result.text.find("96385074") != std::string::npos || result.text == "96385074",
              name, "内容不匹配: " + result.text);
    }

    removeFile("test_ean8.png");
}

static void test_upca() {
    const char* name = "UPC-A 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "012345678905";
    opts.type = imgproc::BarcodeType::UPCA;
    opts.width = 300;
    opts.height = 100;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_upca.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_upca.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "012345678905", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_upca.png");
}

static void test_codabar() {
    const char* name = "Codabar 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "A12345B";
    opts.type = imgproc::BarcodeType::Codabar;
    opts.width = 300;
    opts.height = 100;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_codabar.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_codabar.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        // Codabar 起止符可能被去掉
        CHECK(result.text.find("12345") != std::string::npos,
              name, "内容不匹配: " + result.text);
    }

    removeFile("test_codabar.png");
}

// ============================================================
// 二维码生成测试 (zint → zxing)
// ============================================================

static void test_qrcode_zint() {
    const char* name = "QR Code (zint) 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "https://www.example.com";
    opts.type = imgproc::BarcodeType::QRCode;
    opts.width = 256;
    opts.height = 256;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_qr_zint.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_qr_zint.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "https://www.example.com", name,
              "内容不匹配: " + result.text);
        CHECK(result.type == imgproc::BarcodeType::QRCode, name,
              "类型不匹配");
    }

    removeFile("test_qr_zint.png");
}

static void test_datamatrix() {
    const char* name = "DataMatrix 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "DataMatrix-Test-123";
    opts.type = imgproc::BarcodeType::DataMatrix;
    opts.width = 200;
    opts.height = 200;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_datamatrix.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_datamatrix.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "DataMatrix-Test-123", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_datamatrix.png");
}

static void test_pdf417() {
    const char* name = "PDF417 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "PDF417-Test-Data-12345";
    opts.type = imgproc::BarcodeType::PDF417;
    opts.width = 300;
    opts.height = 150;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_pdf417.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_pdf417.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "PDF417-Test-Data-12345", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_pdf417.png");
}

static void test_aztec() {
    const char* name = "Aztec 生成与读取";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "Aztec-Test-123";
    opts.type = imgproc::BarcodeType::Aztec;
    opts.width = 200;
    opts.height = 200;

    bool genOk = imgproc::generateBarcodeToFile(opts, "test_aztec.png");
    if (!genOk) { FAIL(name, "生成失败"); return; }

    auto result = imgproc::readBarcode("test_aztec.png");
    CHECK(result.success, name, "读取失败");
    if (result.success) {
        CHECK(result.text == "Aztec-Test-123", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_aztec.png");
}

// ============================================================
// 内存生成测试
// ============================================================

static void test_memory_generate_read() {
    const char* name = "内存生成→读取 (Code128)";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "MemoryTest123";
    opts.type = imgproc::BarcodeType::Code128;
    opts.width = 300;
    opts.height = 100;

    std::vector<uint8_t> pngData;
    bool genOk = imgproc::generateBarcodeToMemory(opts, pngData);
    if (!genOk) { FAIL(name, "内存生成失败"); return; }

    // 从内存读取
    auto result = imgproc::readBarcodeFromMemory(pngData.data(), pngData.size());
    CHECK(result.success, name, "内存读取失败");
    if (result.success) {
        CHECK(result.text == "MemoryTest123", name,
              "内容不匹配: " + result.text);
    }
}

static void test_memory_generate_read_qr() {
    const char* name = "内存生成→读取 (QR Code)";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "QRMemoryTest";
    opts.type = imgproc::BarcodeType::QRCode;
    opts.width = 256;
    opts.height = 256;

    std::vector<uint8_t> pngData;
    bool genOk = imgproc::generateBarcodeToMemory(opts, pngData);
    if (!genOk) { FAIL(name, "内存生成失败"); return; }

    auto result = imgproc::readBarcodeFromMemory(pngData.data(), pngData.size());
    CHECK(result.success, name, "内存读取失败");
    if (result.success) {
        CHECK(result.text == "QRMemoryTest", name,
              "内容不匹配: " + result.text);
    }
}

// ============================================================
// 验证测试
// ============================================================

static void test_validate_ok() {
    const char* name = "验证 - EAN13 合法输入";
    TEST(name);

    std::string error;
    bool ok = imgproc::validateBarcodeText("6901234567892", imgproc::BarcodeType::EAN13, error);
    CHECK(ok, name, error);
}

static void test_validate_bad_length() {
    const char* name = "验证 - EAN13 长度错误";
    TEST(name);

    std::string error;
    bool ok = imgproc::validateBarcodeText("123", imgproc::BarcodeType::EAN13, error);
    CHECK(!ok, name, "应该失败");
}

static void test_validate_bad_charset() {
    const char* name = "验证 - EAN13 非数字";
    TEST(name);

    std::string error;
    bool ok = imgproc::validateBarcodeText("abcdefghijklm", imgproc::BarcodeType::EAN13, error);
    CHECK(!ok, name, "应该失败");
}

static void test_validate_empty() {
    const char* name = "验证 - 空文本";
    TEST(name);

    std::string error;
    bool ok = imgproc::validateBarcodeText("", imgproc::BarcodeType::Code128, error);
    CHECK(!ok, name, "应该失败");
}

// ============================================================
// 条码信息测试
// ============================================================

static void test_barcode_info() {
    const char* name = "条码类型信息";
    TEST(name);

    auto types = imgproc::getSupportedBarcodeTypes();
    CHECK(!types.empty(), name, "类型列表为空");
    CHECK(types.size() >= 14, name, "类型数量不足");

    auto info = imgproc::getBarcodeInfo(imgproc::BarcodeType::EAN13);
    CHECK(info != nullptr, name, "EAN13 信息为空");
    if (info) {
        CHECK(info->is2D == false, name, "EAN13 不应是二维码");
        CHECK(info->minLength == 12, name, "EAN13 最小长度应为 12");
    }

    auto qrInfo = imgproc::getBarcodeInfo(imgproc::BarcodeType::QRCode);
    CHECK(qrInfo != nullptr, name, "QR Code 信息为空");
    if (qrInfo) {
        CHECK(qrInfo->is2D == true, name, "QR Code 应该是二维码");
    }
}

// ============================================================
// 边界条件测试
// ============================================================

static void test_read_nonexistent() {
    const char* name = "读取不存在的文件";
    TEST(name);

    auto result = imgproc::readBarcode("nonexistent_file.png");
    CHECK(!result.success, name, "应该返回失败");
}

static void test_read_known_qr() {
    const char* name = "读取已知 QR 码 (qrcodegen 生成)";
    TEST(name);

    // 先用 qrcodegen 生成一个已知可读的 QR 码
    imgproc::QRCodeGenerateOptions qrOpts;
    qrOpts.text = "KnownQRTest";
    qrOpts.width = 256;
    qrOpts.height = 256;
    bool ok = imgproc::generateQRCodeToFile(qrOpts, "test_known_qr_for_barcode.png");
    if (!ok) { FAIL(name, "QR 生成失败"); return; }

    // 用 readQRCode (已验证可用) 读取
    auto qrResult = imgproc::readQRCode("test_known_qr_for_barcode.png");
    CHECK(qrResult.success, name, "readQRCode 读取失败");

    // 用 readBarcode 读取同一个文件
    auto result = imgproc::readBarcode("test_known_qr_for_barcode.png");
    CHECK(result.success, name, "readBarcode 读取失败");
    if (result.success) {
        CHECK(result.text == "KnownQRTest", name,
              "内容不匹配: " + result.text);
    }

    removeFile("test_known_qr_for_barcode.png");
}

static void test_generate_invalid_ean() {
    const char* name = "生成无效 EAN-13";
    TEST(name);

    imgproc::BarcodeGenerateOptions opts;
    opts.text = "abc";  // EAN-13 只支持数字
    opts.type = imgproc::BarcodeType::EAN13;

    bool ok = imgproc::generateBarcodeToFile(opts, "test_invalid.png");
    CHECK(!ok, name, "应该生成失败");

    removeFile("test_invalid.png");
}

// ============================================================
// 主函数
// ============================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "条形码生成与读取单元测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 一维码
    std::cout << "--- 一维码 ---" << std::endl;
    test_code128();
    test_code39();
    test_ean13();
    test_ean8();
    test_upca();
    test_codabar();

    // 二维码
    std::cout << std::endl << "--- 二维码 (zint → zxing) ---" << std::endl;
    test_qrcode_zint();
    test_datamatrix();
    test_pdf417();
    test_aztec();

    // 内存
    std::cout << std::endl << "--- 内存操作 ---" << std::endl;
    test_memory_generate_read();
    test_memory_generate_read_qr();

    // 验证
    std::cout << std::endl << "--- 输入验证 ---" << std::endl;
    test_validate_ok();
    test_validate_bad_length();
    test_validate_bad_charset();
    test_validate_empty();

    // 信息
    std::cout << std::endl << "--- 类型信息 ---" << std::endl;
    test_barcode_info();

    // 边界
    std::cout << std::endl << "--- 边界条件 ---" << std::endl;
    test_read_nonexistent();
    test_read_known_qr();
    test_generate_invalid_ean();

    // 汇总
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "测试结果: " << passed << " 通过, " << failed << " 失败" << std::endl;
    std::cout << "========================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
