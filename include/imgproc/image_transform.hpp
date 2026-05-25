#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

// Interpolation algorithms
enum class Interpolation {
    Nearest,
    Bilinear,
    Bicubic
};

IMGPROC_API bool resizeImage(const ImageBuffer& src, ImageBuffer& dst,
                 int newWidth, int newHeight,
                 Interpolation interp = Interpolation::Bilinear);

IMGPROC_API bool cropImage(const ImageBuffer& src, ImageBuffer& dst,
               int x, int y, int width, int height);

IMGPROC_API bool rotateImage(const ImageBuffer& src, ImageBuffer& dst,
                 double angle, bool expand = true,
                 Interpolation interp = Interpolation::Bilinear);

enum class FlipMode {
    Horizontal,
    Vertical,
    Both
};

IMGPROC_API bool flipImage(const ImageBuffer& src, ImageBuffer& dst, FlipMode mode);

} // namespace imgproc
