#pragma once
#include "imgproc/types.hpp"
#include <vector>
#include <cstdint>

namespace imgproc {

// 便捷函数: 生成二维码到 ImageBuffer (原始像素)
bool generateQRCode(const QRCodeGenerateOptions& opts, ImageBuffer& out);

// 便捷函数: 生成二维码并保存到文件
bool generateQRCodeToFile(const QRCodeGenerateOptions& opts, const std::string& path);

// 便捷函数: 生成二维码到内存 (编码后的文件字节流)
// format 指定输出格式 (BMP/PNG/JPEG), quality 仅对 JPEG 有效
bool generateQRCodeToMemory(const QRCodeGenerateOptions& opts,
                            std::vector<uint8_t>& out,
                            ImageType format = ImageType::PNG,
                            int quality = 85);

// Logo 位置
enum class LogoPosition {
    Center,     // 居中（默认）
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

// 带 Logo 的二维码生成选项
struct QRCodeWithLogoOptions {
    std::string text;              // 要编码的文本
    int eccLevel = 1;              // 纠错等级: 0=L(7%), 1=M(15%), 2=Q(25%), 3=H(30%)
    int margin = 4;                // 静区宽度 (模块数)
    uint32_t fgColor = 0x000000;   // 前景色 (RGB, 默认黑色)
    uint32_t bgColor = 0xFFFFFF;   // 背景色 (RGB, 默认白色)

    // Logo 相关
    ImageBuffer logo;              // Logo 图像 (会被缩放)
    LogoPosition logoPos = LogoPosition::Center;
    float logoScale = 0.2f;        // Logo 占二维码的比例 (0.0-1.0)，默认 20%
    bool logoBorder = true;        // 是否给 Logo 加白色边框
    int borderWidth = 2;           // 边框宽度（模块数）
};

// 生成带 Logo 的二维码
bool generateQRCodeWithLogo(const QRCodeWithLogoOptions& opts, ImageBuffer& out);
bool generateQRCodeWithLogoToFile(const QRCodeWithLogoOptions& opts, const std::string& path);
bool generateQRCodeWithLogoToMemory(const QRCodeWithLogoOptions& opts,
                                     std::vector<uint8_t>& out,
                                     ImageType format = ImageType::PNG,
                                     int quality = 85);

} // namespace imgproc
