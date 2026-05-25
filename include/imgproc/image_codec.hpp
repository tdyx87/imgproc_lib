#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

IMGPROC_API bool convertToJpeg(const std::string& inputPath, const std::string& outputPath, int quality = 85, bool useWindowsAPI = true);
IMGPROC_API bool convertToJpegFromMemory(const uint8_t* data, size_t size, ImageType type,
                             std::vector<uint8_t>& jpegOut, int quality = 85, bool useWindowsAPI = true);
IMGPROC_API ImageType detectImageType(const uint8_t* data, size_t size);
IMGPROC_API ImageType detectImageTypeByExtension(const std::string& path);

} // namespace imgproc
