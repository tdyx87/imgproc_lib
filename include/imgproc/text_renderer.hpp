#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

IMGPROC_API bool renderTextToFile(const TextRenderOptions& opts, const std::string& path, bool useWindowsAPI = true);
IMGPROC_API bool renderTextToMemory(const TextRenderOptions& opts, ImageBuffer& out, bool useWindowsAPI = true);

} // namespace imgproc
