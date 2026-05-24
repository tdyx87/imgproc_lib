#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

// 便捷函数 - 自动选择实现
bool convertToJpeg(const std::string& inputPath, const std::string& outputPath, int quality = 85, bool useWindowsAPI = true);
bool convertToJpegFromMemory(const uint8_t* data, size_t size, ImageType type,
                             std::vector<uint8_t>& jpegOut, int quality = 85, bool useWindowsAPI = true);
ImageType detectImageType(const uint8_t* data, size_t size);
ImageType detectImageTypeByExtension(const std::string& path);

} // namespace imgproc
