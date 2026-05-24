#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

CompressionResult compressRLE(const uint8_t* data, size_t size, int width, int height, PixelFormat format);
CompressionResult compressDeltaRow(const uint8_t* data, size_t size, int width, int height, PixelFormat format);
CompressionResult compressJPEG(const uint8_t* data, size_t size, int width, int height, PixelFormat format, int quality = 75);

bool decompressRLE(const uint8_t* data, size_t size, std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format);
bool decompressDeltaRow(const uint8_t* data, size_t size, std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format);
bool decompressJPEG(const uint8_t* data, size_t size, std::vector<uint8_t>& out, int& width, int& height, PixelFormat& format);

} // namespace imgproc
