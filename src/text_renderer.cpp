#include "imgproc/text_renderer.hpp"
#include "imgproc/platform/win_codec.hpp"
#include "imgproc/platform/cross_codec.hpp"
#include <cstring>
#include <cmath>

namespace imgproc {

bool renderTextToFile(const TextRenderOptions& opts, const std::string& path, bool useWindowsAPI) {
    (void)useWindowsAPI;
    try {
#ifdef _WIN32
        auto renderer = createWinTextRenderer();
#else
        auto renderer = createCrossTextRenderer();
#endif
        if (!renderer) return false;
        return renderer->renderToFile(opts, path);
    } catch (...) {
        return false;
    }
}

bool renderTextToMemory(const TextRenderOptions& opts, ImageBuffer& out, bool useWindowsAPI) {
    (void)useWindowsAPI;
    try {
#ifdef _WIN32
        auto renderer = createWinTextRenderer();
#else
        auto renderer = createCrossTextRenderer();
#endif
        if (!renderer) return false;
        return renderer->renderToMemory(opts, out);
    } catch (...) {
        return false;
    }
}

} // namespace imgproc
