#include "imgproc/image_codec.hpp"
#include "imgproc/platform/win_codec.hpp"
#include "imgproc/platform/cross_codec.hpp"
#include <cstring>
#include <algorithm>
#include <fstream>

namespace imgproc {

ImageType detectImageType(const uint8_t* data, size_t size) {
    if (size < 2) return ImageType::Unknown;

    // JPEG: FF D8 FF
    if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return ImageType::JPEG;
    }

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        return ImageType::PNG;
    }

    // BMP: 42 4D
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') {
        return ImageType::BMP;
    }

    return ImageType::Unknown;
}

ImageType detectImageTypeByExtension(const std::string& path) {
    auto dotPos = path.rfind('.');
    if (dotPos == std::string::npos) return ImageType::Unknown;

    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "jpg" || ext == "jpeg") return ImageType::JPEG;
    if (ext == "png") return ImageType::PNG;
    if (ext == "bmp") return ImageType::BMP;
    return ImageType::Unknown;
}

bool convertToJpeg(const std::string& inputPath, const std::string& outputPath,
                   int quality, bool useWindowsAPI) {
    (void)useWindowsAPI; // 暂时禁用 Windows API，使用跨平台实现
    try {
        auto codec = createCrossCodec();
        if (!codec) return false;

        ImageBuffer img;
        if (!codec->loadFromFile(inputPath, img)) return false;
        return codec->saveToJpegFile(img, outputPath, quality);
    } catch (...) {
        return false;
    }
}

bool convertToJpegFromMemory(const uint8_t* data, size_t size, ImageType type,
                             std::vector<uint8_t>& jpegOut, int quality, bool useWindowsAPI) {
    (void)useWindowsAPI;
    try {
        auto codec = createCrossCodec();
        if (!codec) return false;

        if (type == ImageType::Unknown) {
            type = detectImageType(data, size);
        }
        if (type == ImageType::Unknown) return false;

        ImageBuffer img;
        if (!codec->loadFromMemory(data, size, type, img)) return false;
        return codec->saveToJpegMemory(img, jpegOut, quality);
    } catch (...) {
        return false;
    }
}

} // namespace imgproc
