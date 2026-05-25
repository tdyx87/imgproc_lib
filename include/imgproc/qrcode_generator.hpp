#pragma once
#include "imgproc/types.hpp"
#include <vector>
#include <cstdint>

namespace imgproc {

IMGPROC_API bool generateQRCode(const QRCodeGenerateOptions& opts, ImageBuffer& out);
IMGPROC_API bool generateQRCodeToFile(const QRCodeGenerateOptions& opts, const std::string& path);
IMGPROC_API bool generateQRCodeToMemory(const QRCodeGenerateOptions& opts,
                            std::vector<uint8_t>& out,
                            ImageType format = ImageType::PNG,
                            int quality = 85);

enum class LogoPosition {
    Center,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct QRCodeWithLogoOptions {
    std::string text;
    int eccLevel = 1;
    int margin = 4;
    uint32_t fgColor = 0x000000;
    uint32_t bgColor = 0xFFFFFF;
    ImageBuffer logo;
    LogoPosition logoPos = LogoPosition::Center;
    float logoScale = 0.2f;
    bool logoBorder = true;
    int borderWidth = 2;
};

IMGPROC_API bool generateQRCodeWithLogo(const QRCodeWithLogoOptions& opts, ImageBuffer& out);
IMGPROC_API bool generateQRCodeWithLogoToFile(const QRCodeWithLogoOptions& opts, const std::string& path);
IMGPROC_API bool generateQRCodeWithLogoToMemory(const QRCodeWithLogoOptions& opts,
                                     std::vector<uint8_t>& out,
                                     ImageType format = ImageType::PNG,
                                     int quality = 85);

} // namespace imgproc
