#pragma once
#include "imgproc/types.hpp"

namespace imgproc {

// 插值算法
enum class Interpolation {
    Nearest,    // 最近邻 - 最快，质量一般
    Bilinear,   // 双线性 - 平衡
    Bicubic     // 双三次 - 质量最好，较慢
};

// 图像缩放
// 返回 false 表示失败
bool resizeImage(const ImageBuffer& src, ImageBuffer& dst,
                 int newWidth, int newHeight,
                 Interpolation interp = Interpolation::Bilinear);

// 图像裁剪
// x, y: 起始坐标 (左上角)
// width, height: 裁剪尺寸
bool cropImage(const ImageBuffer& src, ImageBuffer& dst,
               int x, int y, int width, int height);

// 图像旋转
// angle: 角度（逆时针，0-360）
// expand: 是否扩展画布以容纳完整图像
bool rotateImage(const ImageBuffer& src, ImageBuffer& dst,
                 double angle, bool expand = true,
                 Interpolation interp = Interpolation::Bilinear);

// 图像翻转
enum class FlipMode {
    Horizontal, // 水平翻转
    Vertical,   // 垂直翻转
    Both        // 水平和垂直都翻转（180度旋转）
};
bool flipImage(const ImageBuffer& src, ImageBuffer& dst, FlipMode mode);

} // namespace imgproc
