#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

// 便捷函数
QRCodeResult readQRCode(const std::string& imagePath);
QRCodeResult readQRCodeFromMemory(const uint8_t* data, size_t size);

} // namespace imgproc
