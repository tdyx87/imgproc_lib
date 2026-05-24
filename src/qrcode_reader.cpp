#include "imgproc/qrcode_reader.hpp"
#include "imgproc/qrcode_generator.hpp"
#include "imgproc/image_codec.hpp"
#include "imgproc/image_transform.hpp"
#include "imgproc/types.hpp"

#include <ZXing/ReadBarcode.h>
#include <ZXing/BarcodeFormat.h>
#include <ZXing/DecodeHints.h>
#include <ZXing/ImageView.h>
#include <ZXing/Result.h>
#include <qrcodegen.hpp>

#include <fstream>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>

namespace imgproc {

namespace {

// 将 ImageBuffer 转换为灰度数据
std::vector<uint8_t> toGrayscale(const ImageBuffer& img) {
    std::vector<uint8_t> gray(static_cast<size_t>(img.width) * img.height);

    if (img.format == PixelFormat::Grayscale8) {
        if (static_cast<size_t>(img.stride) == gray.size()) {
            std::memcpy(gray.data(), img.data.data(), gray.size());
        } else {
            for (int y = 0; y < img.height; ++y) {
                std::memcpy(gray.data() + y * img.width,
                           img.data.data() + y * img.stride,
                           img.width);
            }
        }
        return gray;
    }

    // 1-bit 索引格式：根据调色板展开
    if (img.format == PixelFormat::Indexed1) {
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                int byteIdx = y * img.stride + (x / 8);
                int bitIdx = 7 - (x % 8);
                int paletteIdx = (img.data[byteIdx] >> bitIdx) & 1;

                // 从调色板获取颜色 (BGRA 格式)
                if (img.palette.size() >= 8) {
                    uint8_t b = img.palette[paletteIdx * 4];
                    uint8_t g = img.palette[paletteIdx * 4 + 1];
                    uint8_t r = img.palette[paletteIdx * 4 + 2];
                    gray[static_cast<size_t>(y) * img.width + x] =
                        static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                } else {
                    // 默认：0=黑，1=白
                    gray[static_cast<size_t>(y) * img.width + x] = (paletteIdx == 0) ? 0 : 255;
                }
            }
        }
        return gray;
    }

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            size_t srcIdx = static_cast<size_t>(y) * img.stride + x * 4;
            uint8_t r = 0, g = 0, b = 0;

            switch (img.format) {
                case PixelFormat::RGB24:
                    srcIdx = static_cast<size_t>(y) * img.stride + x * 3;
                    r = img.data[srcIdx];
                    g = img.data[srcIdx + 1];
                    b = img.data[srcIdx + 2];
                    break;
                case PixelFormat::BGR24:
                    srcIdx = static_cast<size_t>(y) * img.stride + x * 3;
                    b = img.data[srcIdx];
                    g = img.data[srcIdx + 1];
                    r = img.data[srcIdx + 2];
                    break;
                case PixelFormat::RGBA32:
                    r = img.data[srcIdx];
                    g = img.data[srcIdx + 1];
                    b = img.data[srcIdx + 2];
                    break;
                case PixelFormat::BGRA32:
                    b = img.data[srcIdx];
                    g = img.data[srcIdx + 1];
                    r = img.data[srcIdx + 2];
                    break;
                case PixelFormat::Grayscale8:
                    srcIdx = static_cast<size_t>(y) * img.stride + x;
                    gray[static_cast<size_t>(y) * img.width + x] = img.data[srcIdx];
                    continue;
                default:
                    r = g = b = 128;
                    break;
            }

            gray[static_cast<size_t>(y) * img.width + x] =
                static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
        }
    }
    return gray;
}

// 生成 1-bit BMP 数据
static void generate1bitBmp(const std::vector<uint8_t>& gray, int width, int height, ImageBuffer& out) {
    out.width = width;
    out.height = height;
    out.format = PixelFormat::Indexed1;
    out.stride = ((width + 31) / 32) * 4; // BMP 行对齐到 4 字节

    size_t bmpSize = static_cast<size_t>(out.stride) * height;
    out.data.resize(bmpSize, 0xFF); // 白色背景

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (gray[static_cast<size_t>(y) * width + x] < 128) {
                // 黑色像素
                int byteIdx = y * out.stride + (x / 8);
                int bitIdx = 7 - (x % 8);
                out.data[byteIdx] &= static_cast<uint8_t>(~(1 << bitIdx));
            }
        }
    }
}

} // anonymous namespace

// ============================================================
// 便捷函数
// ============================================================

QRCodeResult readQRCode(const std::string& imagePath) {
    auto reader = createQRCodeReader();
    if (!reader) return QRCodeResult{};
    return reader->readFromFile(imagePath);
}

QRCodeResult readQRCodeFromMemory(const uint8_t* data, size_t size) {
    auto reader = createQRCodeReader();
    if (!reader) return QRCodeResult{};
    return reader->readFromMemory(data, size);
}

// ============================================================
// 实现
// ============================================================

std::unique_ptr<IQRCodeReader> createQRCodeReader() {
    class QRCodeReaderImpl : public IQRCodeReader {
    public:
        QRCodeResult readFromFile(const std::string& imagePath) override {
            std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return QRCodeResult{};

            auto fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
            file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

            return readFromMemory(fileData.data(), fileData.size());
        }

        QRCodeResult readFromMemory(const uint8_t* data, size_t size) override {
            QRCodeResult result;

            // 检测图像类型并解码
            ImageType type = detectImageType(data, size);
            if (type == ImageType::Unknown) return result;

            // Windows 优先使用 WinCodec (GDI+ 更稳定)，其他平台使用 CrossCodec
#ifdef _WIN32
            auto codec = createWinCodec();
#else
            auto codec = createCrossCodec();
#endif
            if (!codec) return result;

            ImageBuffer img;
            if (!codec->loadFromMemory(data, size, type, img)) return result;

            // 转换为灰度
            auto gray = toGrayscale(img);

            try {
                using namespace ZXing;

                ImageView imageView(gray.data(), img.width, img.height, ImageFormat::Lum);

                DecodeHints hints;
                // 不限制格式，自动检测所有条码类型

                Result zxResult = ReadBarcode(imageView, hints);

                if (zxResult.isValid()) {
                    result.success = true;
                    result.text = zxResult.text();
                    generate1bitBmp(gray, img.width, img.height, result.bitmap1bit);
                }
            } catch (...) {
                // 解码失败
            }

            return result;
        }

        std::vector<QRCodeResult> readMultipleFromFile(const std::string& imagePath) override {
            std::vector<QRCodeResult> results;

            std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return results;

            auto fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
            file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

            ImageType type = detectImageType(fileData.data(), fileData.size());
            if (type == ImageType::Unknown) return results;

#ifdef _WIN32
            auto codec = createWinCodec();
#else
            auto codec = createCrossCodec();
#endif
            if (!codec) return results;

            ImageBuffer img;
            if (!codec->loadFromMemory(fileData.data(), fileData.size(), type, img)) return results;

            auto gray = toGrayscale(img);

            try {
                using namespace ZXing;

                ImageView imageView(gray.data(), img.width, img.height, ImageFormat::Lum);

                DecodeHints hints;
                // 不限制格式，自动检测所有条码类型

                std::vector<Result> zxResults = ReadBarcodes(imageView, hints);

                for (const auto& zxResult : zxResults) {
                    if (!zxResult.isValid()) continue;

                    QRCodeResult r;
                    r.success = true;
                    r.text = zxResult.text();
                    generate1bitBmp(gray, img.width, img.height, r.bitmap1bit);
                    results.push_back(std::move(r));
                }
            } catch (...) {
                // 解码失败
            }

            return results;
        }
    };

    return std::make_unique<QRCodeReaderImpl>();
}

// ============================================================
// 二维码生成实现
// ============================================================

bool generateQRCode(const QRCodeGenerateOptions& opts, ImageBuffer& out) {
    auto gen = createQRCodeGenerator();
    if (!gen) return false;
    return gen->generate(opts, out);
}

bool generateQRCodeToFile(const QRCodeGenerateOptions& opts, const std::string& path) {
    auto gen = createQRCodeGenerator();
    if (!gen) return false;
    return gen->generateToFile(opts, path);
}

bool generateQRCodeToMemory(const QRCodeGenerateOptions& opts,
                            std::vector<uint8_t>& out,
                            ImageType format,
                            int quality) {
    // 1. 生成二维码到 ImageBuffer
    ImageBuffer img;
    if (!generateQRCode(opts, img)) return false;

    // 2. 编码为目标格式的字节流
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

std::unique_ptr<IQRCodeGenerator> createQRCodeGenerator() {
    class QRCodeGeneratorImpl : public IQRCodeGenerator {
    public:
        bool generate(const QRCodeGenerateOptions& opts, ImageBuffer& out) override {
            try {
                using namespace qrcodegen;

                // 转换纠错等级
                QrCode::Ecc ecc = QrCode::Ecc::MEDIUM;
                switch (opts.eccLevel) {
                    case 0: ecc = QrCode::Ecc::LOW; break;
                    case 1: ecc = QrCode::Ecc::MEDIUM; break;
                    case 2: ecc = QrCode::Ecc::QUARTILE; break;
                    case 3: ecc = QrCode::Ecc::HIGH; break;
                }

                // 生成 QR Code（添加 UTF-8 ECI 段以支持中文等多字节字符）
                std::vector<qrcodegen::QrSegment> segs;
                // ECI 000026 = UTF-8 编码
                segs.push_back(qrcodegen::QrSegment::makeEci(26));
                // 将文本按 UTF-8 字节编码
                std::vector<uint8_t> utf8Bytes;
                for (size_t i = 0; i < opts.text.size(); ++i) {
                    utf8Bytes.push_back(static_cast<uint8_t>(opts.text[i]));
                }
                segs.push_back(qrcodegen::QrSegment::makeBytes(utf8Bytes));

                std::unique_ptr<qrcodegen::QrCode> qr(new qrcodegen::QrCode(
                    qrcodegen::QrCode::encodeSegments(segs, ecc)));
                int qrSize = qr->getSize();

                // 计算缩放因子
                int scale = 1;
                if (opts.width > 0 && opts.height > 0) {
                    // 指定了尺寸：按最小边适配
                    int minSize = std::min(opts.width, opts.height);
                    scale = std::max(1, minSize / qrSize);
                } else {
                    // 未指定尺寸：自动计算，确保每个模块至少 4 像素（保证清晰可扫）
                    scale = 4;
                }

                // 加上静区
                int margin = opts.margin;
                int outputSize = qrSize * scale + margin * 2;

                out.width = outputSize;
                out.height = outputSize;
                out.format = PixelFormat::BGRA32;
                out.stride = outputSize * 4;
                out.data.resize(static_cast<size_t>(out.stride) * outputSize);

                // 解析颜色
                uint8_t fgR = opts.fgColor & 0xFF;
                uint8_t fgG = (opts.fgColor >> 8) & 0xFF;
                uint8_t fgB = (opts.fgColor >> 16) & 0xFF;
                uint8_t bgR = opts.bgColor & 0xFF;
                uint8_t bgG = (opts.bgColor >> 8) & 0xFF;
                uint8_t bgB = (opts.bgColor >> 16) & 0xFF;

                // 填充背景
                for (int y = 0; y < outputSize; ++y) {
                    for (int x = 0; x < outputSize; ++x) {
                        size_t idx = static_cast<size_t>(y * out.stride + x * 4);
                        out.data[idx] = bgB;
                        out.data[idx + 1] = bgG;
                        out.data[idx + 2] = bgR;
                        out.data[idx + 3] = 0xFF;
                    }
                }

                // 绘制二维码
                for (int y = 0; y < qrSize; ++y) {
                    for (int x = 0; x < qrSize; ++x) {
                        if (qr->getModule(x, y)) {
                            for (int dy = 0; dy < scale; ++dy) {
                                for (int dx = 0; dx < scale; ++dx) {
                                    int px = margin + x * scale + dx;
                                    int py = margin + y * scale + dy;
                                    size_t idx = static_cast<size_t>(py * out.stride + px * 4);
                                    out.data[idx] = fgB;
                                    out.data[idx + 1] = fgG;
                                    out.data[idx + 2] = fgR;
                                    out.data[idx + 3] = 0xFF;
                                }
                            }
                        }
                    }
                }

                return true;
            } catch (const std::exception&) {
                return false;
            }
        }

        bool generateToFile(const QRCodeGenerateOptions& opts, const std::string& path) override {
            ImageBuffer img;
            if (!generate(opts, img)) return false;

            // 检查是否需要转换为1-bit格式（根据文件扩展名）
            auto dotPos = path.rfind('.');
            bool want1Bit = false;
            if (dotPos != std::string::npos) {
                std::string ext = path.substr(dotPos + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == "bmp" || ext == "png") {
                    want1Bit = true;
                }
            }

            // 转换为1-bit索引格式（黑白）
            if (want1Bit && img.format == PixelFormat::BGRA32) {
                int newStride = ((img.width + 31) / 32) * 4; // 4字节对齐
                std::vector<uint8_t> newData(newStride * img.height, 0xFF); // 白色背景

                // 前景色和背景色
                uint8_t fgR = opts.fgColor & 0xFF;
                uint8_t fgG = (opts.fgColor >> 8) & 0xFF;
                uint8_t fgB = (opts.fgColor >> 16) & 0xFF;

                for (int y = 0; y < img.height; ++y) {
                    for (int x = 0; x < img.width; ++x) {
                        size_t srcIdx = static_cast<size_t>(y) * img.stride + x * 4;
                        uint8_t b = img.data[srcIdx];
                        uint8_t g = img.data[srcIdx + 1];
                        uint8_t r = img.data[srcIdx + 2];

                        // 判断是否为前景色（黑色）
                        bool isForeground = (r == fgR && g == fgG && b == fgB);
                        if (isForeground) {
                            int byteIdx = y * newStride + (x / 8);
                            int bitIdx = 7 - (x % 8);
                            newData[byteIdx] &= static_cast<uint8_t>(~(1 << bitIdx));
                        }
                    }
                }

                img.data = std::move(newData);
                img.stride = newStride;
                img.format = PixelFormat::Indexed1;

                // 设置调色板（0=前景色黑，1=背景色白）
                img.palette.resize(8); // 2个颜色 * 4字节
                // 索引0 = 前景色（黑）
                img.palette[0] = fgR;
                img.palette[1] = fgG;
                img.palette[2] = fgB;
                img.palette[3] = 0xFF;
                // 索引1 = 背景色（白）
                uint8_t bgR = opts.bgColor & 0xFF;
                uint8_t bgG = (opts.bgColor >> 8) & 0xFF;
                uint8_t bgB = (opts.bgColor >> 16) & 0xFF;
                img.palette[4] = bgR;
                img.palette[5] = bgG;
                img.palette[6] = bgB;
                img.palette[7] = 0xFF;
            }

// 对于1-bit BMP，使用CrossCodec（WinCodec/GDI+不支持1-bit）
            if (img.format == PixelFormat::Indexed1) {
                auto codec = createCrossCodec();
                if (!codec) return false;
                return codec->saveToFile(img, path);
            }

#ifdef _WIN32
            auto codec = createWinCodec();
#else
            auto codec = createCrossCodec();
#endif
            if (!codec) return false;
            return codec->saveToFile(img, path);
        }
    };

    return std::make_unique<QRCodeGeneratorImpl>();
}

// ============================================================
// 带 Logo 的二维码生成
// ============================================================

static void blendLogo(ImageBuffer& qr, const ImageBuffer& logo, int x, int y) {
    // 将 logo 混合到二维码指定位置（alpha 混合）
    int logoW = logo.width;
    int logoH = logo.height;

    for (int ly = 0; ly < logoH; ++ly) {
        for (int lx = 0; lx < logoW; ++lx) {
            int qx = x + lx;
            int qy = y + ly;
            if (qx < 0 || qx >= qr.width || qy < 0 || qy >= qr.height) continue;

            size_t logoIdx = static_cast<size_t>(ly) * logo.stride + lx * 4;
            size_t qrIdx = static_cast<size_t>(qy) * qr.stride + qx * 4;

            // 获取 logo 颜色和 alpha
            uint8_t lB = logo.data[logoIdx];
            uint8_t lG = logo.data[logoIdx + 1];
            uint8_t lR = logo.data[logoIdx + 2];
            uint8_t lA = logo.data[logoIdx + 3];

            if (lA == 0) continue; // 完全透明，跳过

            if (lA == 255) {
                // 完全不透明，直接覆盖
                qr.data[qrIdx] = lB;
                qr.data[qrIdx + 1] = lG;
                qr.data[qrIdx + 2] = lR;
            } else {
                // Alpha 混合
                float alpha = lA / 255.0f;
                qr.data[qrIdx] = static_cast<uint8_t>(lB * alpha + qr.data[qrIdx] * (1 - alpha));
                qr.data[qrIdx + 1] = static_cast<uint8_t>(lG * alpha + qr.data[qrIdx + 1] * (1 - alpha));
                qr.data[qrIdx + 2] = static_cast<uint8_t>(lR * alpha + qr.data[qrIdx + 2] * (1 - alpha));
            }
        }
    }
}

bool generateQRCodeWithLogo(const QRCodeWithLogoOptions& opts, ImageBuffer& out) {
    // 1. 生成普通二维码（使用最高纠错等级以保证 Logo 覆盖后仍可识别）
    QRCodeGenerateOptions qrOpts;
    qrOpts.text = opts.text;
    qrOpts.eccLevel = std::max(opts.eccLevel, 2); // 至少使用 Q 级 (25%) 纠错
    qrOpts.margin = opts.margin;
    qrOpts.fgColor = opts.fgColor;
    qrOpts.bgColor = opts.bgColor;
    // 不指定尺寸，自动计算

    if (!generateQRCode(qrOpts, out)) return false;

    // 2. 如果没有提供 logo，直接返回普通二维码
    if (opts.logo.data.empty()) return true;

    // 3. 将 logo 转换为 BGRA32 格式（如果需要）
    ImageBuffer logoRgba;
    if (opts.logo.format == PixelFormat::BGRA32) {
        logoRgba = opts.logo;
    } else {
        // 简单转换：分配 BGRA 缓冲区并填充
        logoRgba.width = opts.logo.width;
        logoRgba.height = opts.logo.height;
        logoRgba.format = PixelFormat::BGRA32;
        logoRgba.stride = opts.logo.width * 4;
        logoRgba.data.resize(static_cast<size_t>(logoRgba.stride) * logoRgba.height, 255);

        int srcBpp = 4;
        switch (opts.logo.format) {
            case PixelFormat::Grayscale8: srcBpp = 1; break;
            case PixelFormat::RGB24:
            case PixelFormat::BGR24: srcBpp = 3; break;
            case PixelFormat::RGBA32:
            case PixelFormat::BGRA32: srcBpp = 4; break;
            default: srcBpp = 4; break;
        }

        for (int y = 0; y < opts.logo.height; ++y) {
            for (int x = 0; x < opts.logo.width; ++x) {
                size_t srcIdx = static_cast<size_t>(y) * opts.logo.stride + x * srcBpp;
                size_t dstIdx = static_cast<size_t>(y) * logoRgba.stride + x * 4;

                if (opts.logo.format == PixelFormat::Grayscale8) {
                    uint8_t gray = opts.logo.data[srcIdx];
                    logoRgba.data[dstIdx] = gray;
                    logoRgba.data[dstIdx + 1] = gray;
                    logoRgba.data[dstIdx + 2] = gray;
                    logoRgba.data[dstIdx + 3] = 255;
                } else if (opts.logo.format == PixelFormat::RGB24) {
                    logoRgba.data[dstIdx] = opts.logo.data[srcIdx + 2]; // B
                    logoRgba.data[dstIdx + 1] = opts.logo.data[srcIdx + 1]; // G
                    logoRgba.data[dstIdx + 2] = opts.logo.data[srcIdx]; // R
                    logoRgba.data[dstIdx + 3] = 255;
                } else if (opts.logo.format == PixelFormat::BGR24) {
                    logoRgba.data[dstIdx] = opts.logo.data[srcIdx]; // B
                    logoRgba.data[dstIdx + 1] = opts.logo.data[srcIdx + 1]; // G
                    logoRgba.data[dstIdx + 2] = opts.logo.data[srcIdx + 2]; // R
                    logoRgba.data[dstIdx + 3] = 255;
                } else if (opts.logo.format == PixelFormat::RGBA32) {
                    logoRgba.data[dstIdx] = opts.logo.data[srcIdx + 2]; // B
                    logoRgba.data[dstIdx + 1] = opts.logo.data[srcIdx + 1]; // G
                    logoRgba.data[dstIdx + 2] = opts.logo.data[srcIdx]; // R
                    logoRgba.data[dstIdx + 3] = opts.logo.data[srcIdx + 3]; // A
                } else {
                    // BGRA32 or default
                    logoRgba.data[dstIdx] = opts.logo.data[srcIdx];
                    logoRgba.data[dstIdx + 1] = opts.logo.data[srcIdx + 1];
                    logoRgba.data[dstIdx + 2] = opts.logo.data[srcIdx + 2];
                    logoRgba.data[dstIdx + 3] = opts.logo.data[srcIdx + 3];
                }
            }
        }
    }

    // 4. 计算 logo 目标尺寸
    int qrSize = std::min(out.width, out.height);
    int logoTargetSize = static_cast<int>(qrSize * opts.logoScale);
    logoTargetSize = std::max(16, std::min(logoTargetSize, qrSize / 2)); // 限制在 16px 到 50% 之间

    // 5. 缩放 logo
    ImageBuffer logoScaled;
    if (logoRgba.width != logoTargetSize || logoRgba.height != logoTargetSize) {
        if (!resizeImage(logoRgba, logoScaled, logoTargetSize, logoTargetSize, Interpolation::Bilinear)) {
            return false;
        }
    } else {
        logoScaled = logoRgba;
    }

    // 6. 计算 logo 位置
    int logoX, logoY;
    int margin = opts.margin * (qrSize / (opts.text.empty() ? 21 : 25)); // 估算模块大小

    switch (opts.logoPos) {
        case LogoPosition::TopLeft:
            logoX = margin;
            logoY = margin;
            break;
        case LogoPosition::TopRight:
            logoX = out.width - logoTargetSize - margin;
            logoY = margin;
            break;
        case LogoPosition::BottomLeft:
            logoX = margin;
            logoY = out.height - logoTargetSize - margin;
            break;
        case LogoPosition::BottomRight:
            logoX = out.width - logoTargetSize - margin;
            logoY = out.height - logoTargetSize - margin;
            break;
        case LogoPosition::Center:
        default:
            logoX = (out.width - logoTargetSize) / 2;
            logoY = (out.height - logoTargetSize) / 2;
            break;
    }

    // 7. 绘制白色边框（如果需要）
    if (opts.logoBorder && opts.borderWidth > 0) {
        int borderPixels = opts.borderWidth * (qrSize / 25); // 估算
        uint8_t bgB = opts.bgColor & 0xFF;
        uint8_t bgG = (opts.bgColor >> 8) & 0xFF;
        uint8_t bgR = (opts.bgColor >> 16) & 0xFF;

        for (int by = -borderPixels; by < logoTargetSize + borderPixels; ++by) {
            for (int bx = -borderPixels; bx < logoTargetSize + borderPixels; ++bx) {
                int qx = logoX + bx;
                int qy = logoY + by;
                if (qx < 0 || qx >= out.width || qy < 0 || qy >= out.height) continue;
                // 只填充边框区域（不在 logo 内部）
                if (bx >= 0 && bx < logoTargetSize && by >= 0 && by < logoTargetSize) continue;

                size_t qrIdx = static_cast<size_t>(qy) * out.stride + qx * 4;
                out.data[qrIdx] = bgB;
                out.data[qrIdx + 1] = bgG;
                out.data[qrIdx + 2] = bgR;
            }
        }
    }

    // 8. 混合 logo
    blendLogo(out, logoScaled, logoX, logoY);

    return true;
}

bool generateQRCodeWithLogoToFile(const QRCodeWithLogoOptions& opts, const std::string& path) {
    ImageBuffer img;
    if (!generateQRCodeWithLogo(opts, img)) return false;

#ifdef _WIN32
    auto codec = createWinCodec();
#else
    auto codec = createCrossCodec();
#endif
    if (!codec) return false;
    return codec->saveToFile(img, path);
}

bool generateQRCodeWithLogoToMemory(const QRCodeWithLogoOptions& opts,
                                     std::vector<uint8_t>& out,
                                     ImageType format,
                                     int quality) {
    ImageBuffer img;
    if (!generateQRCodeWithLogo(opts, img)) return false;

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

} // namespace imgproc
