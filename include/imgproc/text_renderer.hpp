#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

bool renderTextToFile(const TextRenderOptions& opts, const std::string& path, bool useWindowsAPI = true);
bool renderTextToMemory(const TextRenderOptions& opts, ImageBuffer& out, bool useWindowsAPI = true);

} // namespace imgproc
