#pragma once
#include "imgproc/types.hpp"
#include <string>
#include <vector>

namespace imgproc {

// 条形码类型
enum class BarcodeType {
    // 一维码
    Code128,        // Code 128 (通用，支持 ASCII)
    Code39,         // Code 39 (工业标准)
    Code93,         // Code 93 (紧凑)
    EAN13,          // EAN-13 (商品条码)
    EAN8,           // EAN-8 (短商品条码)
    UPCA,           // UPC-A (北美商品条码)
    UPCE,           // UPC-E (短 UPC)
    Interleaved2of5, // ITF-14 (物流)
    Codabar,        // Codabar (医疗/图书馆)
    
    // 二维码
    QRCode,         // QR Code
    DataMatrix,     // Data Matrix (小尺寸)
    PDF417,         // PDF417 (大容量)
    Aztec,          // Aztec Code
    MaxiCode,       // MaxiCode (物流)
};

// 条形码生成选项
struct BarcodeGenerateOptions {
    std::string text;           // 要编码的文本
    BarcodeType type = BarcodeType::Code128;
    int width = 300;            // 图像宽度
    int height = 100;           // 图像高度 (一维码)
                                // 对于二维码，使用 width 作为边长
    int margin = 10;            // 边距 (像素)
    
    // 样式
    uint32_t fgColor = 0x000000; // 条码颜色 (黑色)
    uint32_t bgColor = 0xFFFFFF; // 背景颜色 (白色)
    bool showText = true;        // 显示文字 (一维码)
    int fontSize = 12;           // 文字大小
    
    // 二维码专用
    int eccLevel = 1;           // 纠错等级 0-3 (仅二维码)
};

// 条形码信息
struct BarcodeInfo {
    BarcodeType type;
    const char* name;
    const char* description;
    bool is2D;                  // 是否为二维码
    int minLength;              // 最小数据长度
    int maxLength;              // 最大数据长度 (-1 表示无限制)
    const char* charset;        // 支持的字符集
};

// 获取支持的条形码类型列表
const std::vector<BarcodeInfo>& getSupportedBarcodeTypes();

// 获取条形码类型信息
const BarcodeInfo* getBarcodeInfo(BarcodeType type);

// 生成条形码
bool generateBarcode(const BarcodeGenerateOptions& opts, ImageBuffer& out);

// 生成条形码到文件
bool generateBarcodeToFile(const BarcodeGenerateOptions& opts, const std::string& path);

// 生成条形码到内存
bool generateBarcodeToMemory(const BarcodeGenerateOptions& opts,
                              std::vector<uint8_t>& out,
                              ImageType format = ImageType::PNG,
                              int quality = 85);

// 验证文本是否适合指定条码类型
bool validateBarcodeText(const std::string& text, BarcodeType type, std::string& error);

// ============================================================
// 条形码读取
// ============================================================

// 条形码读取结果
struct BarcodeReadResult {
    bool success = false;
    std::string text;           // 解码内容
    BarcodeType type;           // 条码类型
    std::string typeName;       // 类型名称 (如 "QR Code", "Code128")
};

// 读取条形码 (自动检测所有格式)
BarcodeReadResult readBarcode(const std::string& imagePath);

// 读取条形码 (指定格式)
BarcodeReadResult readBarcode(const std::string& imagePath, BarcodeType type);

// 从内存读取条形码
BarcodeReadResult readBarcodeFromMemory(const uint8_t* data, size_t size);

// 从内存读取条形码 (指定格式)
BarcodeReadResult readBarcodeFromMemory(const uint8_t* data, size_t size, BarcodeType type);

} // namespace imgproc
