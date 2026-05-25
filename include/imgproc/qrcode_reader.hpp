#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

IMGPROC_API QRCodeResult readQRCode(const std::string& imagePath);
IMGPROC_API QRCodeResult readQRCodeFromMemory(const uint8_t* data, size_t size);

} // namespace imgproc
