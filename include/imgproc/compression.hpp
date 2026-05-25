#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

IMGPROC_API CompressionResult compressRLE(const uint8_t* data, size_t size, int width, int height, PixelFormat format);
IMGPROC_API CompressionResult compressDeltaRow(const uint8_t* data, size_t size, int width, int height, PixelFormat format);
IMGPROC_API CompressionResult compressJPEG(const uint8_t* data, size_t size, int width, int height, PixelFormat format, int quality = 75);

IMGPROC_API bool decompressRLE(const uint8_t* data, size_t size, std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format);
IMGPROC_API bool decompressDeltaRow(const uint8_t* data, size_t size, std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format);
IMGPROC_API bool decompressJPEG(const uint8_t* data, size_t size, std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format);

} // namespace imgproc
