/**
 * @file imgproc_c_api.cpp
 * @brief C API implementation for imgproc_lib
 */

#include "imgproc/imgproc_c_api.h"
#include "imgproc/imgproc.hpp"
#include "imgproc/image_transform.hpp"
#include "imgproc/compression.hpp"
#include "imgproc/barcode_generator.hpp"

#include <cstring>
#include <string>
#include <vector>
#include <memory>

// Thread-local error message
static thread_local std::string g_last_error;

static void set_error(const std::string& msg) {
    g_last_error = msg;
}

static void clear_error() {
    g_last_error.clear();
}

// Convert C format enum to C++
static imgproc::PixelFormat c_to_cpp_format(imgproc_pixel_format_t fmt) {
    switch (fmt) {
        case IMGPROC_FORMAT_GRAYSCALE8: return imgproc::PixelFormat::Grayscale8;
        case IMGPROC_FORMAT_RGB24:      return imgproc::PixelFormat::RGB24;
        case IMGPROC_FORMAT_RGBA32:     return imgproc::PixelFormat::RGBA32;
        case IMGPROC_FORMAT_BGR24:      return imgproc::PixelFormat::BGR24;
        case IMGPROC_FORMAT_BGRA32:     return imgproc::PixelFormat::BGRA32;
        case IMGPROC_FORMAT_INDEXED1:   return imgproc::PixelFormat::Indexed1;
        case IMGPROC_FORMAT_INDEXED4:   return imgproc::PixelFormat::Indexed4;
        case IMGPROC_FORMAT_INDEXED8:   return imgproc::PixelFormat::Indexed8;
        default:                        return imgproc::PixelFormat::BGRA32;
    }
}

static imgproc_pixel_format_t cpp_to_c_format(imgproc::PixelFormat fmt) {
    switch (fmt) {
        case imgproc::PixelFormat::Grayscale8: return IMGPROC_FORMAT_GRAYSCALE8;
        case imgproc::PixelFormat::RGB24:      return IMGPROC_FORMAT_RGB24;
        case imgproc::PixelFormat::RGBA32:     return IMGPROC_FORMAT_RGBA32;
        case imgproc::PixelFormat::BGR24:      return IMGPROC_FORMAT_BGR24;
        case imgproc::PixelFormat::BGRA32:     return IMGPROC_FORMAT_BGRA32;
        case imgproc::PixelFormat::Indexed1:   return IMGPROC_FORMAT_INDEXED1;
        case imgproc::PixelFormat::Indexed4:   return IMGPROC_FORMAT_INDEXED4;
        case imgproc::PixelFormat::Indexed8:   return IMGPROC_FORMAT_INDEXED8;
        default:                               return IMGPROC_FORMAT_UNKNOWN;
    }
}

static imgproc::ImageType c_to_cpp_image_type(imgproc_image_type_t type) {
    switch (type) {
        case IMGPROC_IMAGE_BMP:    return imgproc::ImageType::BMP;
        case IMGPROC_IMAGE_PNG:    return imgproc::ImageType::PNG;
        case IMGPROC_IMAGE_JPEG:   return imgproc::ImageType::JPEG;
        default:                   return imgproc::ImageType::Unknown;
    }
}

static imgproc_image_type_t cpp_to_c_image_type(imgproc::ImageType type) {
    switch (type) {
        case imgproc::ImageType::BMP:    return IMGPROC_IMAGE_BMP;
        case imgproc::ImageType::PNG:    return IMGPROC_IMAGE_PNG;
        case imgproc::ImageType::JPEG:   return IMGPROC_IMAGE_JPEG;
        default:                         return IMGPROC_IMAGE_UNKNOWN;
    }
}

// Copy ImageBuffer from C++ to C (allocates memory)
static void copy_image_buffer_to_c(const imgproc::ImageBuffer& src, imgproc_image_buffer_t* dst) {
    dst->width = src.width;
    dst->height = src.height;
    dst->stride = src.stride;
    dst->format = cpp_to_c_format(src.format);
    dst->data_size = src.data.size();
    dst->data = nullptr;
    dst->palette = nullptr;
    dst->palette_size = 0;

    if (!src.data.empty()) {
        dst->data = (uint8_t*)malloc(src.data.size());
        if (dst->data) {
            memcpy(dst->data, src.data.data(), src.data.size());
        }
    }

    if (!src.palette.empty()) {
        dst->palette_size = src.palette.size();
        dst->palette = (uint8_t*)malloc(src.palette.size());
        if (dst->palette) {
            memcpy(dst->palette, src.palette.data(), src.palette.size());
        }
    }
}

// Free image buffer data
static void free_image_buffer_data(imgproc_image_buffer_t* img) {
    if (img->data) {
        free(img->data);
        img->data = nullptr;
    }
    if (img->palette) {
        free(img->palette);
        img->palette = nullptr;
    }
    img->data_size = 0;
    img->palette_size = 0;
}

// Convert C image buffer to C++ ImageBuffer
static imgproc::ImageBuffer c_to_cpp_image(const imgproc_image_buffer_t* img) {
    imgproc::ImageBuffer cpp_img;
    cpp_img.width = img->width;
    cpp_img.height = img->height;
    cpp_img.stride = img->stride;
    cpp_img.format = c_to_cpp_format(img->format);
    if (img->data && img->data_size > 0) {
        cpp_img.data.assign(img->data, img->data + img->data_size);
    }
    if (img->palette && img->palette_size > 0) {
        cpp_img.palette.assign(img->palette, img->palette + img->palette_size);
    }
    return cpp_img;
}

// Convert C++ ImageBuffer to C image buffer (allocates memory)
static void cpp_to_c_image(const imgproc::ImageBuffer& src, imgproc_image_buffer_t* dst) {
    copy_image_buffer_to_c(src, dst);
}

/* ============================================================
 * Library lifecycle
 * ============================================================ */

extern "C" IMGPROC_API int imgproc_init(void) {
    clear_error();
    // Currently no global initialization required
    return 0;
}

extern "C" IMGPROC_API void imgproc_cleanup(void) {
    clear_error();
    // Currently no global cleanup required
}

/* ============================================================
 * Image buffer management
 * ============================================================ */

extern "C" IMGPROC_API int imgproc_image_alloc(imgproc_image_buffer_t* img, int width, int height, imgproc_pixel_format_t format) {
    if (!img || width <= 0 || height <= 0) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(img, 0, sizeof(*img));

    int bpp = 4; // Default to 32-bit
    switch (format) {
        case IMGPROC_FORMAT_GRAYSCALE8: bpp = 1; break;
        case IMGPROC_FORMAT_RGB24:
        case IMGPROC_FORMAT_BGR24:      bpp = 3; break;
        case IMGPROC_FORMAT_RGBA32:
        case IMGPROC_FORMAT_BGRA32:     bpp = 4; break;
        case IMGPROC_FORMAT_INDEXED1:   bpp = 1; break; // Special handling
        case IMGPROC_FORMAT_INDEXED4:   bpp = 1; break;
        case IMGPROC_FORMAT_INDEXED8:   bpp = 1; break;
        default:                        bpp = 4; break;
    }

    img->width = width;
    img->height = height;
    img->format = format;

    if (format == IMGPROC_FORMAT_INDEXED1) {
        img->stride = ((width + 31) / 32) * 4;
    } else if (format == IMGPROC_FORMAT_INDEXED4) {
        img->stride = ((width + 7) / 8) * 4;
    } else if (format == IMGPROC_FORMAT_INDEXED8) {
        img->stride = ((width + 3) / 4) * 4;
    } else {
        img->stride = width * bpp;
    }

    img->data_size = static_cast<size_t>(img->stride) * height;
    img->data = (uint8_t*)calloc(1, img->data_size);

    if (!img->data) {
        set_error("Failed to allocate image data");
        return -1;
    }

    clear_error();
    return 0;
}

extern "C" IMGPROC_API void imgproc_image_free(imgproc_image_buffer_t* img) {
    if (img) {
        free_image_buffer_data(img);
        memset(img, 0, sizeof(*img));
    }
}

extern "C" IMGPROC_API void imgproc_qrcode_result_free(imgproc_qrcode_result_t* result) {
    if (result) {
        if (result->text) {
            free(result->text);
            result->text = nullptr;
        }
        free_image_buffer_data(&result->bitmap_1bit);
        memset(result, 0, sizeof(*result));
    }
}

/* ============================================================
 * Image codec
 * ============================================================ */

extern "C" IMGPROC_API int imgproc_load_image(const char* path, imgproc_image_buffer_t* img) {
    if (!path || !img) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(img, 0, sizeof(*img));

    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        set_error("Failed to create codec");
        return -1;
    }

    imgproc::ImageBuffer cpp_img;
    if (!codec->loadFromFile(path, cpp_img)) {
        set_error("Failed to load image");
        return -1;
    }

    copy_image_buffer_to_c(cpp_img, img);
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_qrcode_generate_memory(const imgproc_qrcode_options_t* opts,
                                                            imgproc_image_type_t format,
                                                            uint8_t** out_data,
                                                            size_t* out_size,
                                                            int jpeg_quality) {
    if (!opts || !out_data || !out_size) {
        set_error("Invalid parameters");
        return -1;
    }

    *out_data = nullptr;
    *out_size = 0;

    imgproc::QRCodeGenerateOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.width = opts->width;
    cpp_opts.height = opts->height;
    cpp_opts.margin = opts->margin;
    cpp_opts.eccLevel = opts->ecc_level;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;

    std::vector<uint8_t> buffer;
    if (!imgproc::generateQRCodeToMemory(cpp_opts, buffer, c_to_cpp_image_type(format), jpeg_quality)) {
        set_error("Failed to generate QR code to memory");
        return -1;
    }

    *out_size = buffer.size();
    *out_data = (uint8_t*)malloc(buffer.size());
    if (!*out_data) {
        set_error("Failed to allocate output buffer");
        return -1;
    }

    memcpy(*out_data, buffer.data(), buffer.size());
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_save_image(const imgproc_image_buffer_t* img, const char* path, int jpeg_quality) {
    if (!img || !path) {
        set_error("Invalid parameters");
        return -1;
    }

    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        set_error("Failed to create codec");
        return -1;
    }

    imgproc::ImageBuffer cpp_img;
    cpp_img.width = img->width;
    cpp_img.height = img->height;
    cpp_img.stride = img->stride;
    cpp_img.format = c_to_cpp_format(img->format);

    if (img->data && img->data_size > 0) {
        cpp_img.data.assign(img->data, img->data + img->data_size);
    }
    if (img->palette && img->palette_size > 0) {
        cpp_img.palette.assign(img->palette, img->palette + img->palette_size);
    }

    if (!codec->saveToFile(cpp_img, path, jpeg_quality)) {
        set_error("Failed to save image");
        return -1;
    }

    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_load_image_memory(const uint8_t* data, size_t size, imgproc_image_type_t type, imgproc_image_buffer_t* img) {
    if (!data || size == 0 || !img) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(img, 0, sizeof(*img));

    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        set_error("Failed to create codec");
        return -1;
    }

    imgproc::ImageBuffer cpp_img;
    if (!codec->loadFromMemory(data, size, c_to_cpp_image_type(type), cpp_img)) {
        set_error("Failed to load image from memory");
        return -1;
    }

    copy_image_buffer_to_c(cpp_img, img);
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_save_image_memory(const imgproc_image_buffer_t* img, imgproc_image_type_t type, uint8_t** out_data, size_t* out_size, int jpeg_quality) {
    if (!img || !out_data || !out_size) {
        set_error("Invalid parameters");
        return -1;
    }

    *out_data = nullptr;
    *out_size = 0;

    auto codec = imgproc::createCrossCodec();
    if (!codec) {
        set_error("Failed to create codec");
        return -1;
    }

    imgproc::ImageBuffer cpp_img;
    cpp_img.width = img->width;
    cpp_img.height = img->height;
    cpp_img.stride = img->stride;
    cpp_img.format = c_to_cpp_format(img->format);

    if (img->data && img->data_size > 0) {
        cpp_img.data.assign(img->data, img->data + img->data_size);
    }
    if (img->palette && img->palette_size > 0) {
        cpp_img.palette.assign(img->palette, img->palette + img->palette_size);
    }

    std::vector<uint8_t> buffer;
    bool success = false;

    switch (type) {
        case IMGPROC_IMAGE_BMP:
            success = codec->saveToBmpMemory(cpp_img, buffer);
            break;
        case IMGPROC_IMAGE_PNG:
            success = codec->saveToPngMemory(cpp_img, buffer);
            break;
        case IMGPROC_IMAGE_JPEG:
            success = codec->saveToJpegMemory(cpp_img, buffer, jpeg_quality);
            break;
        default:
            set_error("Unsupported image type");
            return -1;
    }

    if (!success) {
        set_error("Failed to save image to memory");
        return -1;
    }

    *out_size = buffer.size();
    *out_data = (uint8_t*)malloc(buffer.size());
    if (!*out_data) {
        set_error("Failed to allocate output buffer");
        return -1;
    }

    memcpy(*out_data, buffer.data(), buffer.size());
    clear_error();
    return 0;
}

/* ============================================================
 * QR Code generation
 * ============================================================ */

extern "C" IMGPROC_API int imgproc_qrcode_generate_file(const imgproc_qrcode_options_t* opts, const char* path) {
    if (!opts || !path) {
        set_error("Invalid parameters");
        return -1;
    }

    imgproc::QRCodeGenerateOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.width = opts->width;
    cpp_opts.height = opts->height;
    cpp_opts.margin = opts->margin;
    cpp_opts.eccLevel = opts->ecc_level;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;

    if (!imgproc::generateQRCodeToFile(cpp_opts, path)) {
        set_error("Failed to generate QR code");
        return -1;
    }

    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_qrcode_generate_image(const imgproc_qrcode_options_t* opts, imgproc_image_buffer_t* img) {
    if (!opts || !img) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(img, 0, sizeof(*img));

    imgproc::QRCodeGenerateOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.width = opts->width;
    cpp_opts.height = opts->height;
    cpp_opts.margin = opts->margin;
    cpp_opts.eccLevel = opts->ecc_level;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;

    imgproc::ImageBuffer cpp_img;
    if (!imgproc::generateQRCode(cpp_opts, cpp_img)) {
        set_error("Failed to generate QR code");
        return -1;
    }

    copy_image_buffer_to_c(cpp_img, img);
    clear_error();
    return 0;
}

/* ============================================================
 * QR Code reading
 * ============================================================ */

extern "C" IMGPROC_API int imgproc_qrcode_read_file(const char* path, imgproc_qrcode_result_t* result) {
    if (!path || !result) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    auto cpp_result = imgproc::readQRCode(path);
    if (!cpp_result.success) {
        set_error("Failed to read QR code");
        return -1;
    }

    result->success = 1;
    result->qr_version = cpp_result.qrVersion;
    result->error_correction_level = cpp_result.errorCorrectionLevel;

    if (!cpp_result.text.empty()) {
        result->text = (char*)malloc(cpp_result.text.size() + 1);
        if (result->text) {
            strcpy(result->text, cpp_result.text.c_str());
        }
    }

    copy_image_buffer_to_c(cpp_result.bitmap1bit, &result->bitmap_1bit);
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_qrcode_read_image(const imgproc_image_buffer_t* img, imgproc_qrcode_result_t* result) {
    if (!img || !result) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    imgproc::ImageBuffer cpp_img;
    cpp_img.width = img->width;
    cpp_img.height = img->height;
    cpp_img.stride = img->stride;
    cpp_img.format = c_to_cpp_format(img->format);

    if (img->data && img->data_size > 0) {
        cpp_img.data.assign(img->data, img->data + img->data_size);
    }
    if (img->palette && img->palette_size > 0) {
        cpp_img.palette.assign(img->palette, img->palette + img->palette_size);
    }

    auto reader = imgproc::createQRCodeReader();
    if (!reader) {
        set_error("Failed to create QR code reader");
        return -1;
    }

    auto cpp_result = reader->readFromMemory(cpp_img.data.data(), cpp_img.data.size());
    // Note: readFromMemory expects file data, not raw pixel data
    // This is a limitation - for raw pixels, we'd need a different approach

    set_error("Reading from raw image buffer not fully supported");
    return -1;
}

extern "C" IMGPROC_API int imgproc_qrcode_read_memory(const uint8_t* data, size_t size, imgproc_image_type_t type, imgproc_qrcode_result_t* result) {
    if (!data || size == 0 || !result) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    auto reader = imgproc::createQRCodeReader();
    if (!reader) {
        set_error("Failed to create QR code reader");
        return -1;
    }

    auto cpp_result = reader->readFromMemory(data, size);
    if (!cpp_result.success) {
        set_error("Failed to read QR code");
        return -1;
    }

    result->success = 1;
    result->qr_version = cpp_result.qrVersion;
    result->error_correction_level = cpp_result.errorCorrectionLevel;

    if (!cpp_result.text.empty()) {
        result->text = (char*)malloc(cpp_result.text.size() + 1);
        if (result->text) {
            strcpy(result->text, cpp_result.text.c_str());
        }
    }

    copy_image_buffer_to_c(cpp_result.bitmap1bit, &result->bitmap_1bit);
    clear_error();
    return 0;
}

/* ============================================================
 * Text rendering
 * ============================================================ */

extern "C" IMGPROC_API int imgproc_text_render_file(const imgproc_text_options_t* opts, const char* path) {
    if (!opts || !path) {
        set_error("Invalid parameters");
        return -1;
    }

    imgproc::TextRenderOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.fontPath = opts->font_path ? opts->font_path : "";
    cpp_opts.fontSize = opts->font_size;
    cpp_opts.dpi = opts->dpi;
    cpp_opts.width = opts->width;
    cpp_opts.height = opts->height;
    cpp_opts.bitsPerPixel = opts->bits_per_pixel;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;
    cpp_opts.antiAlias = opts->anti_alias != 0;

    if (!imgproc::renderTextToFile(cpp_opts, path)) {
        set_error("Failed to render text");
        return -1;
    }

    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_text_render_image(const imgproc_text_options_t* opts, imgproc_image_buffer_t* img) {
    if (!opts || !img) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(img, 0, sizeof(*img));

    imgproc::TextRenderOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.fontPath = opts->font_path ? opts->font_path : "";
    cpp_opts.fontSize = opts->font_size;
    cpp_opts.dpi = opts->dpi;
    cpp_opts.width = opts->width;
    cpp_opts.height = opts->height;
    cpp_opts.bitsPerPixel = opts->bits_per_pixel;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;
    cpp_opts.antiAlias = opts->anti_alias != 0;

    imgproc::ImageBuffer cpp_img;
    if (!imgproc::renderTextToMemory(cpp_opts, cpp_img)) {
        set_error("Failed to render text");
        return -1;
    }

    copy_image_buffer_to_c(cpp_img, img);
    clear_error();
    return 0;
}

/* ============================================================
 * Utility functions
 * ============================================================ */

extern "C" IMGPROC_API imgproc_image_type_t imgproc_detect_type_by_extension(const char* path) {
    if (!path) return IMGPROC_IMAGE_UNKNOWN;
    return cpp_to_c_image_type(imgproc::detectImageTypeByExtension(path));
}

extern "C" IMGPROC_API imgproc_image_type_t imgproc_detect_type_from_data(const uint8_t* data, size_t size) {
    if (!data || size == 0) return IMGPROC_IMAGE_UNKNOWN;
    return cpp_to_c_image_type(imgproc::detectImageType(data, size));
}

extern "C" IMGPROC_API const char* imgproc_version(void) {
    return "1.0.0";
}

extern "C" IMGPROC_API const char* imgproc_last_error(void) {
    return g_last_error.empty() ? nullptr : g_last_error.c_str();
}

/* ============================================================
 * Image transform C API implementation
 * ============================================================ */

static imgproc::Interpolation c_to_cpp_interp(imgproc_interpolation_t interp) {
    switch (interp) {
        case IMGPROC_INTERP_NEAREST:  return imgproc::Interpolation::Nearest;
        case IMGPROC_INTERP_BILINEAR: return imgproc::Interpolation::Bilinear;
        case IMGPROC_INTERP_BICUBIC:  return imgproc::Interpolation::Bicubic;
        default: return imgproc::Interpolation::Bilinear;
    }
}

static imgproc::FlipMode c_to_cpp_flip(imgproc_flip_mode_t mode) {
    switch (mode) {
        case IMGPROC_FLIP_HORIZONTAL: return imgproc::FlipMode::Horizontal;
        case IMGPROC_FLIP_VERTICAL:   return imgproc::FlipMode::Vertical;
        case IMGPROC_FLIP_BOTH:       return imgproc::FlipMode::Both;
        default: return imgproc::FlipMode::Horizontal;
    }
}

extern "C" IMGPROC_API int imgproc_resize_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                                  int new_width, int new_height, imgproc_interpolation_t interp) {
    if (!src || !dst || new_width <= 0 || new_height <= 0) {
        set_error("Invalid parameters");
        return -1;
    }

    auto cpp_src = c_to_cpp_image(src);
    imgproc::ImageBuffer cpp_dst;

    if (!imgproc::resizeImage(cpp_src, cpp_dst, new_width, new_height, c_to_cpp_interp(interp))) {
        set_error("Resize failed");
        return -1;
    }

    cpp_to_c_image(cpp_dst, dst);
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_crop_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                                int x, int y, int w, int h) {
    if (!src || !dst || w <= 0 || h <= 0) {
        set_error("Invalid parameters");
        return -1;
    }

    auto cpp_src = c_to_cpp_image(src);
    imgproc::ImageBuffer cpp_dst;

    if (!imgproc::cropImage(cpp_src, cpp_dst, x, y, w, h)) {
        set_error("Crop failed");
        return -1;
    }

    cpp_to_c_image(cpp_dst, dst);
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_rotate_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                                  float angle, int expand, imgproc_interpolation_t interp) {
    if (!src || !dst) {
        set_error("Invalid parameters");
        return -1;
    }

    auto cpp_src = c_to_cpp_image(src);
    imgproc::ImageBuffer cpp_dst;

    if (!imgproc::rotateImage(cpp_src, cpp_dst, angle, expand != 0, c_to_cpp_interp(interp))) {
        set_error("Rotate failed");
        return -1;
    }

    cpp_to_c_image(cpp_dst, dst);
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_flip_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                                imgproc_flip_mode_t mode) {
    if (!src || !dst) {
        set_error("Invalid parameters");
        return -1;
    }

    auto cpp_src = c_to_cpp_image(src);
    imgproc::ImageBuffer cpp_dst;

    if (!imgproc::flipImage(cpp_src, cpp_dst, c_to_cpp_flip(mode))) {
        set_error("Flip failed");
        return -1;
    }

    cpp_to_c_image(cpp_dst, dst);
    clear_error();
    return 0;
}

/* ============================================================
 * Compression C API implementation
 * ============================================================ */

static imgproc::CompressionType c_to_cpp_compress(imgproc_compress_type_t type) {
    switch (type) {
        case IMGPROC_COMPRESS_RLE:        return imgproc::CompressionType::RLE;
        case IMGPROC_COMPRESS_DELTA_ROW:  return imgproc::CompressionType::DeltaRow;
        case IMGPROC_COMPRESS_JPEG:       return imgproc::CompressionType::JPEG;
        default: return imgproc::CompressionType::None;
    }
}

extern "C" IMGPROC_API int imgproc_compress(const imgproc_image_buffer_t* img, imgproc_compress_type_t type,
                                              imgproc_compress_result_t* result, int jpeg_quality) {
    if (!img || !result) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    auto cpp_img = c_to_cpp_image(img);
    imgproc::CompressionResult cpp_result;

    switch (type) {
        case IMGPROC_COMPRESS_RLE:
            cpp_result = imgproc::compressRLE(cpp_img.data.data(), cpp_img.data.size(),
                                               cpp_img.width, cpp_img.height, cpp_img.format);
            break;
        case IMGPROC_COMPRESS_DELTA_ROW:
            cpp_result = imgproc::compressDeltaRow(cpp_img.data.data(), cpp_img.data.size(),
                                                    cpp_img.width, cpp_img.height, cpp_img.format);
            break;
        case IMGPROC_COMPRESS_JPEG:
            cpp_result = imgproc::compressJPEG(cpp_img.data.data(), cpp_img.data.size(),
                                                cpp_img.width, cpp_img.height, cpp_img.format,
                                                jpeg_quality > 0 ? jpeg_quality : 75);
            break;
        default:
            set_error("Unsupported compression type");
            return -1;
    }

    result->type = type;
    result->data_size = cpp_result.data.size();
    result->ratio = static_cast<float>(cpp_result.compressionRatio);
    result->elapsed_ms = cpp_result.elapsedMs;

    if (result->data_size > 0) {
        result->data = static_cast<uint8_t*>(malloc(result->data_size));
        if (!result->data) {
            set_error("Memory allocation failed");
            return -1;
        }
        std::memcpy(result->data, cpp_result.data.data(), result->data_size);
    }

    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_decompress(const uint8_t* compressed, size_t compressed_size,
                                                imgproc_compress_type_t type,
                                                int width, int height, imgproc_pixel_format_t format,
                                                imgproc_image_buffer_t* img) {
    if (!compressed || compressed_size == 0 || !img || width <= 0 || height <= 0) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(img, 0, sizeof(*img));

    auto cpp_format = c_to_cpp_format(format);
    std::vector<uint8_t> out_data;
    int out_w = width, out_h = height;
    imgproc::PixelFormat out_fmt = cpp_format;

    switch (type) {
        case IMGPROC_COMPRESS_RLE:
            if (!imgproc::decompressRLE(compressed, compressed_size, out_data, out_w, out_h, out_fmt)) {
                set_error("RLE decompression failed");
                return -1;
            }
            break;
        case IMGPROC_COMPRESS_DELTA_ROW:
            if (!imgproc::decompressDeltaRow(compressed, compressed_size, out_data, out_w, out_h, out_fmt)) {
                set_error("DeltaRow decompression failed");
                return -1;
            }
            break;
        case IMGPROC_COMPRESS_JPEG:
            if (!imgproc::decompressJPEG(compressed, compressed_size, out_data, out_w, out_h, out_fmt)) {
                set_error("JPEG decompression failed");
                return -1;
            }
            break;
        default:
            set_error("Unsupported compression type");
            return -1;
    }

    if (out_data.empty()) {
        set_error("Decompression produced no output");
        return -1;
    }

    img->width = out_w;
    img->height = out_h;
    img->format = cpp_to_c_format(out_fmt);

    // 计算 stride
    int bpp = 0;
    switch (cpp_format) {
        case imgproc::PixelFormat::Grayscale8: bpp = 1; break;
        case imgproc::PixelFormat::RGB24: case imgproc::PixelFormat::BGR24: bpp = 3; break;
        case imgproc::PixelFormat::RGBA32: case imgproc::PixelFormat::BGRA32: bpp = 4; break;
        default: bpp = 4; break;
    }
    img->stride = img->width * bpp;
    img->data_size = out_data.size();
    img->data = static_cast<uint8_t*>(malloc(img->data_size));
    if (!img->data) {
        set_error("Memory allocation failed");
        return -1;
    }
    std::memcpy(img->data, out_data.data(), img->data_size);

    clear_error();
    return 0;
}

extern "C" IMGPROC_API void imgproc_compress_result_free(imgproc_compress_result_t* result) {
    if (result) {
        if (result->data) { free(result->data); result->data = nullptr; }
        result->data_size = 0;
    }
}

/* ============================================================
 * Barcode generation C API implementation
 * ============================================================ */

static imgproc::BarcodeType c_to_cpp_barcode(imgproc_barcode_type_t type) {
    switch (type) {
        case IMGPROC_BARCODE_CODE128:    return imgproc::BarcodeType::Code128;
        case IMGPROC_BARCODE_CODE39:     return imgproc::BarcodeType::Code39;
        case IMGPROC_BARCODE_CODE93:     return imgproc::BarcodeType::Code93;
        case IMGPROC_BARCODE_EAN13:      return imgproc::BarcodeType::EAN13;
        case IMGPROC_BARCODE_EAN8:       return imgproc::BarcodeType::EAN8;
        case IMGPROC_BARCODE_UPCA:       return imgproc::BarcodeType::UPCA;
        case IMGPROC_BARCODE_UPCE:       return imgproc::BarcodeType::UPCE;
        case IMGPROC_BARCODE_ITF:        return imgproc::BarcodeType::Interleaved2of5;
        case IMGPROC_BARCODE_CODABAR:    return imgproc::BarcodeType::Codabar;
        case IMGPROC_BARCODE_QRCODE:     return imgproc::BarcodeType::QRCode;
        case IMGPROC_BARCODE_DATAMATRIX: return imgproc::BarcodeType::DataMatrix;
        case IMGPROC_BARCODE_PDF417:     return imgproc::BarcodeType::PDF417;
        case IMGPROC_BARCODE_AZTEC:      return imgproc::BarcodeType::Aztec;
        case IMGPROC_BARCODE_MAXICODE:   return imgproc::BarcodeType::MaxiCode;
        default: return imgproc::BarcodeType::Code128;
    }
}

static imgproc_image_type_t format_to_c_type(const std::string& ext) {
    if (ext == "bmp" || ext == "BMP") return IMGPROC_IMAGE_BMP;
    if (ext == "png" || ext == "PNG") return IMGPROC_IMAGE_PNG;
    if (ext == "jpg" || ext == "jpeg" || ext == "JPG" || ext == "JPEG") return IMGPROC_IMAGE_JPEG;
    return IMGPROC_IMAGE_UNKNOWN;
}

extern "C" IMGPROC_API int imgproc_barcode_generate_file(const imgproc_barcode_options_t* opts, const char* path) {
    if (!opts || !path) {
        set_error("Invalid parameters");
        return -1;
    }

    imgproc::BarcodeGenerateOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.type = c_to_cpp_barcode(opts->type);
    cpp_opts.width = opts->width > 0 ? opts->width : 300;
    cpp_opts.height = opts->height > 0 ? opts->height : 150;
    cpp_opts.margin = opts->margin;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;
    cpp_opts.showText = opts->show_text != 0;
    cpp_opts.fontSize = opts->font_size;

    if (!imgproc::generateBarcodeToFile(cpp_opts, path)) {
        set_error("Barcode generation failed");
        return -1;
    }

    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_barcode_generate_image(const imgproc_barcode_options_t* opts, imgproc_image_buffer_t* img) {
    if (!opts || !img) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(img, 0, sizeof(*img));

    imgproc::BarcodeGenerateOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.type = c_to_cpp_barcode(opts->type);
    cpp_opts.width = opts->width > 0 ? opts->width : 300;
    cpp_opts.height = opts->height > 0 ? opts->height : 150;
    cpp_opts.margin = opts->margin;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;
    cpp_opts.showText = opts->show_text != 0;
    cpp_opts.fontSize = opts->font_size;

    imgproc::ImageBuffer cpp_img;
    if (!imgproc::generateBarcode(cpp_opts, cpp_img)) {
        set_error("Barcode generation failed");
        return -1;
    }

    cpp_to_c_image(cpp_img, img);
    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_barcode_generate_memory(const imgproc_barcode_options_t* opts,
                                                             imgproc_image_type_t format,
                                                             uint8_t** out_data, size_t* out_size,
                                                             int jpeg_quality) {
    if (!opts || !out_data || !out_size) {
        set_error("Invalid parameters");
        return -1;
    }

    *out_data = nullptr;
    *out_size = 0;

    imgproc::BarcodeGenerateOptions cpp_opts;
    cpp_opts.text = opts->text ? opts->text : "";
    cpp_opts.type = c_to_cpp_barcode(opts->type);
    cpp_opts.width = opts->width > 0 ? opts->width : 300;
    cpp_opts.height = opts->height > 0 ? opts->height : 150;
    cpp_opts.margin = opts->margin;
    cpp_opts.fgColor = opts->fg_color;
    cpp_opts.bgColor = opts->bg_color;
    cpp_opts.showText = opts->show_text != 0;
    cpp_opts.fontSize = opts->font_size;

    std::vector<uint8_t> encoded;
    imgproc::ImageType imgType;
    switch (format) {
        case IMGPROC_IMAGE_BMP:  imgType = imgproc::ImageType::BMP; break;
        case IMGPROC_IMAGE_PNG:  imgType = imgproc::ImageType::PNG; break;
        case IMGPROC_IMAGE_JPEG: imgType = imgproc::ImageType::JPEG; break;
        default:
            set_error("Unsupported format");
            return -1;
    }

    if (!imgproc::generateBarcodeToMemory(cpp_opts, encoded, imgType, jpeg_quality > 0 ? jpeg_quality : 90)) {
        set_error("Barcode generation to memory failed");
        return -1;
    }

    *out_size = encoded.size();
    *out_data = static_cast<uint8_t*>(malloc(*out_size));
    if (!*out_data) {
        set_error("Memory allocation failed");
        return -1;
    }
    std::memcpy(*out_data, encoded.data(), *out_size);

    clear_error();
    return 0;
}

/* ============================================================
 * Barcode reading C API implementation
 * ============================================================ */

extern "C" IMGPROC_API int imgproc_barcode_read_file(const char* path, imgproc_barcode_result_t* result) {
    if (!path || !result) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    auto cpp_result = imgproc::readBarcode(std::string(path));
    if (!cpp_result.success) {
        set_error("Barcode read failed");
        return -1;
    }

    result->success = 1;
    result->text = strdup(cpp_result.text.c_str());
    result->type = static_cast<imgproc_barcode_type_t>(cpp_result.type);
    auto info = imgproc::getBarcodeInfo(cpp_result.type);
    result->type_name = info ? info->name : "Unknown";

    clear_error();
    return 0;
}

extern "C" IMGPROC_API int imgproc_barcode_read_memory(const uint8_t* data, size_t size,
                                                         imgproc_image_type_t type,
                                                         imgproc_barcode_result_t* result) {
    if (!data || size == 0 || !result) {
        set_error("Invalid parameters");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    auto cpp_result = imgproc::readBarcodeFromMemory(data, size);
    if (!cpp_result.success) {
        set_error("Barcode read from memory failed");
        return -1;
    }

    result->success = 1;
    result->text = strdup(cpp_result.text.c_str());
    result->type = static_cast<imgproc_barcode_type_t>(cpp_result.type);
    auto info = imgproc::getBarcodeInfo(cpp_result.type);
    result->type_name = info ? info->name : "Unknown";

    clear_error();
    return 0;
}

extern "C" IMGPROC_API void imgproc_barcode_result_free(imgproc_barcode_result_t* result) {
    if (result) {
        if (result->text) { free(result->text); result->text = nullptr; }
    }
}
