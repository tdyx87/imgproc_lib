#include "imgproc/barcode_generator.hpp"
#include "imgproc/image_codec.hpp"
#include "imgproc/qrcode_reader.hpp"
#include <zint.h>
#include <ZXing/ReadBarcode.h>
#include <ZXing/BarcodeFormat.h>
#include <ZXing/DecodeHints.h>
#include <ZXing/ImageView.h>
#include <ZXing/Result.h>
#include <cstring>
#include <map>
#include <fstream>

namespace imgproc {

// 条形码类型映射表
static const std::map<BarcodeType, int> zintSymbologyMap = {
    {BarcodeType::Code128,        BARCODE_CODE128},
    {BarcodeType::Code39,         BARCODE_CODE39},
    {BarcodeType::Code93,         BARCODE_CODE93},
    {BarcodeType::EAN13,          BARCODE_EANX},
    {BarcodeType::EAN8,           BARCODE_EANX},
    {BarcodeType::UPCA,           BARCODE_UPCA},
    {BarcodeType::UPCE,           BARCODE_UPCE},
    {BarcodeType::Interleaved2of5, BARCODE_ITF14},
    {BarcodeType::Codabar,        BARCODE_CODABAR},
    {BarcodeType::QRCode,         BARCODE_QRCODE},
    {BarcodeType::DataMatrix,     BARCODE_DATAMATRIX},
    {BarcodeType::PDF417,         BARCODE_PDF417},
    {BarcodeType::Aztec,          BARCODE_AZTEC},
    {BarcodeType::MaxiCode,       BARCODE_MAXICODE},
};

// 条形码信息表
static const std::vector<BarcodeInfo> barcodeInfoList = {
    {BarcodeType::Code128,        "Code 128",       "通用条形码，支持全 ASCII",       false, 1, -1,   "ASCII"},
    {BarcodeType::Code39,         "Code 39",        "工业标准，支持数字大写字母",    false, 1, -1,   "0-9 A-Z - . $ / + % space"},
    {BarcodeType::Code93,         "Code 93",        "Code 39 的紧凑版本",            false, 1, -1,   "ASCII 子集"},
    {BarcodeType::EAN13,          "EAN-13",         "国际商品条码 (13位)",           false, 12, 13,  "0-9"},
    {BarcodeType::EAN8,           "EAN-8",          "短商品条码 (8位)",              false, 7,  8,  "0-9"},
    {BarcodeType::UPCA,           "UPC-A",          "北美商品条码 (12位)",           false, 11, 12, "0-9"},
    {BarcodeType::UPCE,           "UPC-E",          "短 UPC 条码 (6-8位)",           false, 6,  8,  "0-9"},
    {BarcodeType::Interleaved2of5,"ITF-14",         "物流包装条码",                  false, 13, 14, "0-9"},
    {BarcodeType::Codabar,        "Codabar",        "医疗/图书馆条码",               false, 1, -1,   "0-9 - $ : / . +"},
    {BarcodeType::QRCode,         "QR Code",        "二维码，支持中文",              true,  1, -1,   "任意"},
    {BarcodeType::DataMatrix,     "Data Matrix",    "小尺寸二维码",                  true,  1, -1,   "ASCII"},
    {BarcodeType::PDF417,         "PDF417",         "大容量二维码",                  true,  1, -1,   "任意"},
    {BarcodeType::Aztec,          "Aztec Code",     "高容量二维码",                  true,  1, -1,   "任意"},
    {BarcodeType::MaxiCode,       "MaxiCode",       "物流专用二维码",                true,  1, 93,  "ASCII"},
};

const std::vector<BarcodeInfo>& getSupportedBarcodeTypes() {
    return barcodeInfoList;
}

const BarcodeInfo* getBarcodeInfo(BarcodeType type) {
    for (const auto& info : barcodeInfoList) {
        if (info.type == type) {
            return &info;
        }
    }
    return nullptr;
}

bool validateBarcodeText(const std::string& text, BarcodeType type, std::string& error) {
    if (text.empty()) {
        error = "文本不能为空";
        return false;
    }
    
    auto info = getBarcodeInfo(type);
    if (!info) {
        error = "未知的条码类型";
        return false;
    }
    
    // 检查长度限制
    if (info->maxLength > 0 && static_cast<int>(text.length()) > info->maxLength) {
        error = "文本过长，最大 " + std::to_string(info->maxLength) + " 字符";
        return false;
    }
    if (static_cast<int>(text.length()) < info->minLength) {
        error = "文本过短，最小 " + std::to_string(info->minLength) + " 字符";
        return false;
    }
    
    // 检查字符集
    switch (type) {
        case BarcodeType::EAN13:
        case BarcodeType::EAN8:
        case BarcodeType::UPCA:
        case BarcodeType::UPCE:
        case BarcodeType::Interleaved2of5:
            for (char c : text) {
                if (!std::isdigit(c)) {
                    error = "此条码类型仅支持数字";
                    return false;
                }
            }
            break;
        case BarcodeType::Code39:
            for (char c : text) {
                if (!std::isdigit(c) && !std::isupper(c) && 
                    c != '-' && c != '.' && c != '$' && c != '/' && c != '+' && c != '%' && c != ' ') {
                    error = "Code 39 仅支持数字、大写字母和特定符号";
                    return false;
                }
            }
            break;
        default:
            break;
    }
    
    return true;
}

bool generateBarcode(const BarcodeGenerateOptions& opts, ImageBuffer& out) {
    // 验证文本
    std::string error;
    if (!validateBarcodeText(opts.text, opts.type, error)) {
        return false;
    }
    
    // 查找 Zint 符号类型
    auto it = zintSymbologyMap.find(opts.type);
    if (it == zintSymbologyMap.end()) {
        return false;
    }
    
    // 创建 Zint 符号
    struct zint_symbol* symbol = ZBarcode_Create();
    if (!symbol) {
        return false;
    }
    
    // 设置符号类型
    symbol->symbology = it->second;
    
    // 设置输入数据
    if (ZBarcode_Encode(symbol, reinterpret_cast<const unsigned char*>(opts.text.c_str()), 
                        static_cast<int>(opts.text.length())) != 0) {
        // 编码失败
        ZBarcode_Delete(symbol);
        return false;
    }
    
    // 设置输出选项
    symbol->show_hrt = opts.showText ? 1 : 0;
    symbol->whitespace_width = 0;
    symbol->border_width = 0;
    symbol->output_options = 0;
    
    // 设置颜色 (zint 使用十六进制字符串格式 "RRGGBB")
    char fgHex[16], bgHex[16];
    snprintf(fgHex, sizeof(fgHex), "%02X%02X%02X",
             (opts.fgColor >> 16) & 0xFF, (opts.fgColor >> 8) & 0xFF, opts.fgColor & 0xFF);
    snprintf(bgHex, sizeof(bgHex), "%02X%02X%02X",
             (opts.bgColor >> 16) & 0xFF, (opts.bgColor >> 8) & 0xFF, opts.bgColor & 0xFF);
    strncpy(symbol->fgcolour, fgHex, 7);
    strncpy(symbol->bgcolour, bgHex, 7);
    
    // 设置尺寸
    symbol->scale = 1.0f;
    if (getBarcodeInfo(opts.type) && getBarcodeInfo(opts.type)->is2D) {
        symbol->width = opts.width;
        symbol->height = opts.width;  // 二维码是正方形
    } else {
        symbol->height = opts.height;
    }
    
    // 生成位图
    int bufferResult = ZBarcode_Buffer(symbol, 0);
    if (bufferResult != 0) {
        ZBarcode_Delete(symbol);
        return false;
    }
    
    if (symbol->bitmap_width <= 0 || symbol->bitmap_height <= 0 || symbol->bitmap == nullptr) {
        ZBarcode_Delete(symbol);
        return false;
    }
    
    // 转换为 ImageBuffer
    int width = symbol->bitmap_width;
    int height = symbol->bitmap_height;
    
    out.width = width;
    out.height = height;
    out.format = PixelFormat::BGR24;
    out.stride = width * 3;
    out.data.resize(static_cast<size_t>(out.stride) * height);
    
    // Zint 生成的是 RGB，需要转换为 BGR
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIdx = (y * width + x) * 3;
            int dstIdx = y * out.stride + x * 3;
            // Zint: RGB, ImageBuffer: BGR
            out.data[dstIdx] = symbol->bitmap[srcIdx + 2];     // B
            out.data[dstIdx + 1] = symbol->bitmap[srcIdx + 1]; // G
            out.data[dstIdx + 2] = symbol->bitmap[srcIdx];     // R
        }
    }
    
    ZBarcode_Delete(symbol);
    return true;
}

bool generateBarcodeToFile(const BarcodeGenerateOptions& opts, const std::string& path) {
    ImageBuffer img;
    if (!generateBarcode(opts, img)) {
        return false;
    }
    
#ifdef _WIN32
    auto codec = createWinCodec();
#else
    auto codec = createCrossCodec();
#endif
    if (!codec) return false;
    
    return codec->saveToFile(img, path);
}

bool generateBarcodeToMemory(const BarcodeGenerateOptions& opts,
                              std::vector<uint8_t>& out,
                              ImageType format,
                              int quality) {
    ImageBuffer img;
    if (!generateBarcode(opts, img)) {
        return false;
    }
    
#ifdef _WIN32
    auto codec = createWinCodec();
#else
    auto codec = createCrossCodec();
#endif
    if (!codec) return false;
    
    switch (format) {
        case ImageType::PNG:
            return codec->saveToPngMemory(img, out);
        case ImageType::JPEG:
            return codec->saveToJpegMemory(img, out, quality);
        case ImageType::BMP:
            return codec->saveToBmpMemory(img, out);
        default:
            return codec->saveToPngMemory(img, out);
    }
}

// ============================================================
// 条形码读取
// ============================================================

// ZXing BarcodeFormat 转换为 BarcodeType
static BarcodeType zxFormatToBarcodeType(ZXing::BarcodeFormat format) {
    switch (format) {
        case ZXing::BarcodeFormat::QRCode:    return BarcodeType::QRCode;
        case ZXing::BarcodeFormat::DataMatrix: return BarcodeType::DataMatrix;
        case ZXing::BarcodeFormat::PDF417:     return BarcodeType::PDF417;
        case ZXing::BarcodeFormat::Aztec:      return BarcodeType::Aztec;
        case ZXing::BarcodeFormat::MaxiCode:   return BarcodeType::MaxiCode;
        case ZXing::BarcodeFormat::Code128:    return BarcodeType::Code128;
        case ZXing::BarcodeFormat::Code39:     return BarcodeType::Code39;
        case ZXing::BarcodeFormat::Code93:     return BarcodeType::Code93;
        case ZXing::BarcodeFormat::EAN13:      return BarcodeType::EAN13;
        case ZXing::BarcodeFormat::EAN8:       return BarcodeType::EAN8;
        case ZXing::BarcodeFormat::UPCA:       return BarcodeType::UPCA;
        case ZXing::BarcodeFormat::UPCE:       return BarcodeType::UPCE;
        case ZXing::BarcodeFormat::ITF:        return BarcodeType::Interleaved2of5;
        case ZXing::BarcodeFormat::Codabar:    return BarcodeType::Codabar;
        default: return BarcodeType::Code128;
    }
}

// BarcodeType 转换为 ZXing BarcodeFormat
static ZXing::BarcodeFormat barcodeTypeToZxFormat(BarcodeType type) {
    switch (type) {
        case BarcodeType::QRCode:          return ZXing::BarcodeFormat::QRCode;
        case BarcodeType::DataMatrix:      return ZXing::BarcodeFormat::DataMatrix;
        case BarcodeType::PDF417:          return ZXing::BarcodeFormat::PDF417;
        case BarcodeType::Aztec:           return ZXing::BarcodeFormat::Aztec;
        case BarcodeType::MaxiCode:        return ZXing::BarcodeFormat::MaxiCode;
        case BarcodeType::Code128:         return ZXing::BarcodeFormat::Code128;
        case BarcodeType::Code39:          return ZXing::BarcodeFormat::Code39;
        case BarcodeType::Code93:          return ZXing::BarcodeFormat::Code93;
        case BarcodeType::EAN13:           return ZXing::BarcodeFormat::EAN13;
        case BarcodeType::EAN8:            return ZXing::BarcodeFormat::EAN8;
        case BarcodeType::UPCA:            return ZXing::BarcodeFormat::UPCA;
        case BarcodeType::UPCE:            return ZXing::BarcodeFormat::UPCE;
        case BarcodeType::Interleaved2of5: return ZXing::BarcodeFormat::ITF;
        case BarcodeType::Codabar:         return ZXing::BarcodeFormat::Codabar;
        default: return ZXing::BarcodeFormat::None;
    }
}

// 将 ImageBuffer 转为灰度
static std::vector<uint8_t> toGrayscale(const ImageBuffer& img) {
    std::vector<uint8_t> gray(img.width * img.height);
    int bpp = 4;
    switch (img.format) {
        case PixelFormat::Grayscale8: bpp = 1; break;
        case PixelFormat::RGB24:
        case PixelFormat::BGR24: bpp = 3; break;
        default: bpp = 4; break;
    }
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            size_t idx = static_cast<size_t>(y) * img.stride + x * bpp;
            if (bpp == 1) {
                gray[y * img.width + x] = img.data[idx];
            } else if (bpp == 3) {
                // BGR -> Gray
                gray[y * img.width + x] = static_cast<uint8_t>(
                    img.data[idx] * 114 + img.data[idx + 1] * 587 + img.data[idx + 2] * 299) / 1000;
            } else {
                // BGRA -> Gray
                gray[y * img.width + x] = static_cast<uint8_t>(
                    img.data[idx] * 114 + img.data[idx + 1] * 587 + img.data[idx + 2] * 299) / 1000;
            }
        }
    }
    return gray;
}

static BarcodeReadResult readBarcodeInternal(const uint8_t* imageData, size_t imageSize,
                                               ImageType type, BarcodeType filterType) {
    BarcodeReadResult result;

#ifdef _WIN32
    auto codec = createWinCodec();
#else
    auto codec = createCrossCodec();
#endif
    if (!codec) return result;

    ImageBuffer img;
    if (!codec->loadFromMemory(imageData, imageSize, type, img)) {
        return result;
    }

    auto gray = toGrayscale(img);

    try {
        using namespace ZXing;

        ImageView imageView(gray.data(), img.width, img.height, ImageFormat::Lum);

        DecodeHints hints;
        if (filterType != BarcodeType::Code128) {
            // 指定格式
            hints.setFormats(barcodeTypeToZxFormat(filterType));
        }
        // 否则不设置，自动检测所有格式

        Result zxResult = ReadBarcode(imageView, hints);

        if (zxResult.isValid()) {
            result.success = true;
            result.text = zxResult.text();
            result.type = zxFormatToBarcodeType(zxResult.format());
            auto info = getBarcodeInfo(result.type);
            result.typeName = info ? info->name : "Unknown";
        } else {
        }
    } catch (...) {
        // 解码失败
    }

    return result;
}

BarcodeReadResult readBarcode(const std::string& imagePath) {
    BarcodeReadResult result;

    // 复用 readQRCode 的读取逻辑（它内部已支持所有格式）
    auto qrResult = readQRCode(imagePath);
    if (qrResult.success) {
        result.success = true;
        result.text = qrResult.text;
        result.type = BarcodeType::QRCode;
        result.typeName = "Auto";
    }

    return result;
}

BarcodeReadResult readBarcode(const std::string& imagePath, BarcodeType type) {
    std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return BarcodeReadResult();

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

    ImageType imgType = detectImageType(fileData.data(), fileData.size());
    if (imgType == ImageType::Unknown) return BarcodeReadResult();

    return readBarcodeInternal(fileData.data(), fileData.size(), imgType, type);
}

BarcodeReadResult readBarcodeFromMemory(const uint8_t* data, size_t size) {
    BarcodeReadResult result;

    auto qrResult = readQRCodeFromMemory(data, size);
    if (qrResult.success) {
        result.success = true;
        result.text = qrResult.text;
        result.type = BarcodeType::QRCode;
        result.typeName = "Auto";
    }

    return result;
}

BarcodeReadResult readBarcodeFromMemory(const uint8_t* data, size_t size, BarcodeType type) {
    // 暂不支持指定类型过滤，直接调用自动检测版本
    return readBarcodeFromMemory(data, size);
}

} // namespace imgproc
