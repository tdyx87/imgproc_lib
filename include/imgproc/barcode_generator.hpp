#pragma once
#include "imgproc/types.hpp"
#include <string>
#include <vector>

namespace imgproc {

enum class BarcodeType {
    Code128,
    Code39,
    Code93,
    EAN13,
    EAN8,
    UPCA,
    UPCE,
    Interleaved2of5,
    Codabar,
    QRCode,
    DataMatrix,
    PDF417,
    Aztec,
    MaxiCode,
};

struct BarcodeGenerateOptions {
    std::string text;
    BarcodeType type = BarcodeType::Code128;
    int width = 300;
    int height = 100;
    int margin = 10;
    uint32_t fgColor = 0x000000;
    uint32_t bgColor = 0xFFFFFF;
    bool showText = true;
    int fontSize = 12;
    int eccLevel = 1;
};

struct BarcodeInfo {
    BarcodeType type;
    const char* name;
    const char* description;
    bool is2D;
    int minLength;
    int maxLength;
    const char* charset;
};

IMGPROC_API const std::vector<BarcodeInfo>& getSupportedBarcodeTypes();
IMGPROC_API const BarcodeInfo* getBarcodeInfo(BarcodeType type);

IMGPROC_API bool generateBarcode(const BarcodeGenerateOptions& opts, ImageBuffer& out);
IMGPROC_API bool generateBarcodeToFile(const BarcodeGenerateOptions& opts, const std::string& path);
IMGPROC_API bool generateBarcodeToMemory(const BarcodeGenerateOptions& opts,
                              std::vector<uint8_t>& out,
                              ImageType format = ImageType::PNG,
                              int quality = 85);
IMGPROC_API bool validateBarcodeText(const std::string& text, BarcodeType type, std::string& error);

struct BarcodeReadResult {
    bool success = false;
    std::string text;
    BarcodeType type;
    std::string typeName;
};

IMGPROC_API BarcodeReadResult readBarcode(const std::string& imagePath);
IMGPROC_API BarcodeReadResult readBarcode(const std::string& imagePath, BarcodeType type);
IMGPROC_API BarcodeReadResult readBarcodeFromMemory(const uint8_t* data, size_t size);
IMGPROC_API BarcodeReadResult readBarcodeFromMemory(const uint8_t* data, size_t size, BarcodeType type);

} // namespace imgproc
