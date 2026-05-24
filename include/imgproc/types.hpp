#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace imgproc {

enum class PixelFormat {
    Grayscale8,   // 8-bit 灰度
    RGB24,        // 24-bit RGB
    RGBA32,       // 32-bit RGBA
    BGR24,        // 24-bit BGR (Windows native)
    BGRA32,       // 32-bit BGRA (Windows native)
    Indexed1,     // 1-bit 索引 (黑白)
    Indexed4,     // 4-bit 索引
    Indexed8,     // 8-bit 索引
};

enum class ImageType {
    Unknown,
    BMP,
    PNG,
    JPEG,
};

struct ImageBuffer {
    int width = 0;
    int height = 0;
    int stride = 0;          // 每行字节数
    PixelFormat format = PixelFormat::RGB24;
    std::vector<uint8_t> data;

    // 调色板 (用于索引格式)
    std::vector<uint8_t> palette;

    size_t dataSize() const { return data.size(); }
    bool empty() const { return data.empty(); }
    void clear() { width = height = stride = 0; data.clear(); palette.clear(); }
};

enum class CompressionType {
    None,
    RLE,
    DeltaRow,
    JPEG,
};

struct CompressionResult {
    CompressionType type = CompressionType::None;
    std::vector<uint8_t> data;
    double compressionRatio = 0.0; // 原始大小 / 压缩后大小
    double elapsedMs = 0.0;        // 耗时(毫秒)
};

struct QRCodeResult {
    bool success = false;
    std::string text;
    ImageBuffer bitmap1bit;  // 1-bit BMP 格式的二维码图像
    int qrVersion = 0;
    int errorCorrectionLevel = 0;
};

struct TextRenderOptions {
    std::string text;
    std::string fontPath;    // 字体文件路径 (空则用默认)
    int fontSize = 24;       // 字体大小 (像素, 基于 DPI 缩放)
    int dpi = 96;            // DPI (默认 96, 支持 72/150/200/300 等)
    int width = 0;           // 0 = 自动计算
    int height = 0;
    PixelFormat outputFormat = PixelFormat::RGB24;
    int bitsPerPixel = 0;    // 0=自动, 1/4/8=索引格式, 24=RGB, 32=RGBA
    uint32_t fgColor = 0x000000; // 前景色 (RGB)
    uint32_t bgColor = 0xFFFFFF; // 背景色
    bool antiAlias = true;
    int lineHeight = 0;      // 行高 (像素, 0=自动, 使用字体的 1.2 倍)
    int maxWidth = 0;        // 最大行宽 (像素, 0=不限制, 启用自动换行)
    int alignment = 0;       // 对齐方式: 0=左对齐, 1=居中, 2=右对齐
};

// 接口基类
class IImageCodec {
public:
    virtual ~IImageCodec() = default;

    // 格式转换: 从文件读取
    virtual bool loadFromFile(const std::string& path, ImageBuffer& out) = 0;
    // 格式转换: 从内存读取
    virtual bool loadFromMemory(const uint8_t* data, size_t size, ImageType type, ImageBuffer& out) = 0;
    // 保存为 JPEG 文件
    virtual bool saveToJpegFile(const ImageBuffer& img, const std::string& path, int quality = 85) = 0;
    // 保存为 JPEG 内存
    virtual bool saveToJpegMemory(const ImageBuffer& img, std::vector<uint8_t>& out, int quality = 85) = 0;
    // 保存为 BMP 文件 (含 1-bit)
    virtual bool saveToBmpFile(const ImageBuffer& img, const std::string& path) = 0;
    // 保存为 BMP 内存
    virtual bool saveToBmpMemory(const ImageBuffer& img, std::vector<uint8_t>& out) = 0;
    // 保存为 PNG 文件
    virtual bool saveToPngFile(const ImageBuffer& img, const std::string& path) = 0;
    // 保存为 PNG 内存
    virtual bool saveToPngMemory(const ImageBuffer& img, std::vector<uint8_t>& out) = 0;
    // 智能保存: 根据文件扩展名自动选择格式 (.jpg/.jpeg/.bmp/.png)
    virtual bool saveToFile(const ImageBuffer& img, const std::string& path, int jpegQuality = 85) = 0;
};

class IQRCodeReader {
public:
    virtual ~IQRCodeReader() = default;
    virtual QRCodeResult readFromFile(const std::string& imagePath) = 0;
    virtual QRCodeResult readFromMemory(const uint8_t* data, size_t size) = 0;
    virtual std::vector<QRCodeResult> readMultipleFromFile(const std::string& imagePath) = 0;
};

// 二维码生成选项
struct QRCodeGenerateOptions {
    std::string text;          // 要编码的文本
    int width = 256;           // 输出图片宽度 (像素)
    int height = 256;          // 输出图片高度 (像素)
    int margin = 4;            // 静区宽度 (模块数)
    int eccLevel = 1;          // 纠错等级: 0=L(7%), 1=M(15%), 2=Q(25%), 3=H(30%)
    uint32_t fgColor = 0x000000; // 前景色 (RGB, 默认黑色)
    uint32_t bgColor = 0xFFFFFF; // 背景色 (RGB, 默认白色)
};

class IQRCodeGenerator {
public:
    virtual ~IQRCodeGenerator() = default;
    // 生成二维码到 ImageBuffer
    virtual bool generate(const QRCodeGenerateOptions& opts, ImageBuffer& out) = 0;
    // 生成二维码并保存到文件
    virtual bool generateToFile(const QRCodeGenerateOptions& opts, const std::string& path) = 0;
};

class ITextRenderer {
public:
    virtual ~ITextRenderer() = default;
    virtual bool renderToFile(const TextRenderOptions& opts, const std::string& path) = 0;
    virtual bool renderToMemory(const TextRenderOptions& opts, ImageBuffer& out) = 0;
};

class ICompression {
public:
    virtual ~ICompression() = default;
    virtual CompressionResult compress(const uint8_t* data, size_t size, int width, int height, PixelFormat format) = 0;
    virtual bool decompress(const uint8_t* compressed, size_t compressedSize,
                           std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format) = 0;
};

// 工厂函数
std::unique_ptr<IImageCodec> createWinCodec();
std::unique_ptr<IImageCodec> createCrossCodec();
std::unique_ptr<IQRCodeReader> createQRCodeReader();
std::unique_ptr<IQRCodeGenerator> createQRCodeGenerator();
std::unique_ptr<ITextRenderer> createWinTextRenderer();
std::unique_ptr<ITextRenderer> createCrossTextRenderer();
std::unique_ptr<ICompression> createRLECompression();
std::unique_ptr<ICompression> createDeltaRowCompression();
std::unique_ptr<ICompression> createJPEGCompression(int quality = 75);

} // namespace imgproc
