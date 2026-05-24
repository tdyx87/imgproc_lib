/**
 * @file imgproc_c_api.h
 * @brief C API for imgproc_lib (DLL export/import compatible)
 *
 * This header provides a C-compatible interface for the imgproc library,
 * allowing it to be used from C code or other languages via FFI.
 */

#ifndef IMGPROC_C_API_H
#define IMGPROC_C_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Platform-specific export/import macros
 * ============================================================ */

#ifdef _WIN32
    #ifdef IMGPROC_BUILD_DLL
        #define IMGPROC_API __declspec(dllexport)
    #elif defined(IMGPROC_USE_DLL)
        #define IMGPROC_API __declspec(dllimport)
    #else
        #define IMGPROC_API
    #endif
#else
    #define IMGPROC_API __attribute__((visibility("default")))
#endif

/* ============================================================
 * Type definitions
 * ============================================================ */

typedef enum {
    IMGPROC_FORMAT_UNKNOWN = 0,
    IMGPROC_FORMAT_GRAYSCALE8,
    IMGPROC_FORMAT_RGB24,
    IMGPROC_FORMAT_RGBA32,
    IMGPROC_FORMAT_BGR24,
    IMGPROC_FORMAT_BGRA32,
    IMGPROC_FORMAT_INDEXED1,
    IMGPROC_FORMAT_INDEXED4,
    IMGPROC_FORMAT_INDEXED8
} imgproc_pixel_format_t;

typedef enum {
    IMGPROC_IMAGE_UNKNOWN = 0,
    IMGPROC_IMAGE_BMP,
    IMGPROC_IMAGE_PNG,
    IMGPROC_IMAGE_JPEG
} imgproc_image_type_t;

typedef struct {
    int width;
    int height;
    int stride;
    imgproc_pixel_format_t format;
    uint8_t* data;
    size_t data_size;
    uint8_t* palette;
    size_t palette_size;
} imgproc_image_buffer_t;

typedef struct {
    int success;
    char* text;
    imgproc_image_buffer_t bitmap_1bit;
    int qr_version;
    int error_correction_level;
} imgproc_qrcode_result_t;

typedef struct {
    const char* text;
    int width;
    int height;
    int margin;
    int ecc_level;  /* 0=L, 1=M, 2=Q, 3=H */
    uint32_t fg_color;
    uint32_t bg_color;
} imgproc_qrcode_options_t;

typedef struct {
    const char* text;
    const char* font_path;
    int font_size;
    int dpi;
    int width;
    int height;
    int bits_per_pixel;
    uint32_t fg_color;
    uint32_t bg_color;
    int anti_alias;
    int line_height;
    int max_width;
    int alignment;  /* 0=left, 1=center, 2=right */
} imgproc_text_options_t;

/* ============================================================
 * Library lifecycle
 * ============================================================ */

/**
 * @brief Initialize the library (required before first use)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_init(void);

/**
 * @brief Cleanup library resources
 */
IMGPROC_API void imgproc_cleanup(void);

/* ============================================================
 * Image buffer management
 * ============================================================ */

/**
 * @brief Allocate an image buffer
 */
IMGPROC_API int imgproc_image_alloc(imgproc_image_buffer_t* img, int width, int height, imgproc_pixel_format_t format);

/**
 * @brief Free an image buffer
 */
IMGPROC_API void imgproc_image_free(imgproc_image_buffer_t* img);

/**
 * @brief Free QR code result (including text and bitmap)
 */
IMGPROC_API void imgproc_qrcode_result_free(imgproc_qrcode_result_t* result);

/* ============================================================
 * Image codec
 * ============================================================ */

/**
 * @brief Load image from file
 * @param path File path (encoding depends on platform, UTF-8 recommended)
 * @param img Output image buffer (must be freed with imgproc_image_free)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_load_image(const char* path, imgproc_image_buffer_t* img);

/**
 * @brief Save image to file (format determined by extension)
 * @param img Image buffer
 * @param path Output file path
 * @param jpeg_quality JPEG quality (1-100), ignored for other formats
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_save_image(const imgproc_image_buffer_t* img, const char* path, int jpeg_quality);

/**
 * @brief Load image from memory buffer
 */
IMGPROC_API int imgproc_load_image_memory(const uint8_t* data, size_t size, imgproc_image_type_t type, imgproc_image_buffer_t* img);

/**
 * @brief Save image to memory buffer
 */
IMGPROC_API int imgproc_save_image_memory(const imgproc_image_buffer_t* img, imgproc_image_type_t type, uint8_t** out_data, size_t* out_size, int jpeg_quality);

/* ============================================================
 * QR Code generation
 * ============================================================ */

/**
 * @brief Generate QR code to file
 * @param opts QR code generation options
 * @param path Output file path (format by extension: .bmp, .png, .jpg)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_qrcode_generate_file(const imgproc_qrcode_options_t* opts, const char* path);

/**
 * @brief Generate QR code to image buffer (raw pixels)
 * @param opts QR code generation options
 * @param img Output image buffer (must be freed with imgproc_image_free)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_qrcode_generate_image(const imgproc_qrcode_options_t* opts, imgproc_image_buffer_t* img);

/**
 * @brief Generate QR code to memory (encoded file bytes)
 * @param opts QR code generation options
 * @param format Output format (BMP/PNG/JPEG)
 * @param out_data Receives pointer to allocated buffer (must be freed with free())
 * @param out_size Receives buffer size
 * @param jpeg_quality JPEG quality (1-100), ignored for other formats
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_qrcode_generate_memory(const imgproc_qrcode_options_t* opts,
                                                imgproc_image_type_t format,
                                                uint8_t** out_data,
                                                size_t* out_size,
                                                int jpeg_quality);

/* ============================================================
 * QR Code reading
 * ============================================================ */

/**
 * @brief Read QR code from image file
 * @param path Image file path
 * @param result Output result (must be freed with imgproc_qrcode_result_free)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_qrcode_read_file(const char* path, imgproc_qrcode_result_t* result);

/**
 * @brief Read QR code from image buffer
 */
IMGPROC_API int imgproc_qrcode_read_image(const imgproc_image_buffer_t* img, imgproc_qrcode_result_t* result);

/**
 * @brief Read QR code from memory buffer
 */
IMGPROC_API int imgproc_qrcode_read_memory(const uint8_t* data, size_t size, imgproc_image_type_t type, imgproc_qrcode_result_t* result);

/* ============================================================
 * Text rendering
 * ============================================================ */

/**
 * @brief Render text to file
 * @param opts Text rendering options
 * @param path Output file path
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_text_render_file(const imgproc_text_options_t* opts, const char* path);

/**
 * @brief Render text to image buffer
 */
IMGPROC_API int imgproc_text_render_image(const imgproc_text_options_t* opts, imgproc_image_buffer_t* img);

/* ============================================================
 * Image transform
 * ============================================================ */

typedef enum {
    IMGPROC_INTERP_NEAREST = 0,
    IMGPROC_INTERP_BILINEAR,
    IMGPROC_INTERP_BICUBIC
} imgproc_interpolation_t;

typedef enum {
    IMGPROC_FLIP_HORIZONTAL = 0,
    IMGPROC_FLIP_VERTICAL,
    IMGPROC_FLIP_BOTH
} imgproc_flip_mode_t;

/**
 * @brief Resize image
 * @param src Source image
 * @param dst Destination image (will be allocated)
 * @param new_width Target width
 * @param new_height Target height
 * @param interp Interpolation method
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_resize_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                      int new_width, int new_height, imgproc_interpolation_t interp);

/**
 * @brief Crop image
 * @param src Source image
 * @param dst Destination image (will be allocated)
 * @param x Start X coordinate
 * @param y Start Y coordinate
 * @param w Crop width
 * @param h Crop height
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_crop_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                    int x, int y, int w, int h);

/**
 * @brief Rotate image
 * @param src Source image
 * @param dst Destination image (will be allocated)
 * @param angle Rotation angle in degrees (counter-clockwise)
 * @param expand Expand canvas to fit entire rotated image (1=yes, 0=no)
 * @param interp Interpolation method
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_rotate_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                      float angle, int expand, imgproc_interpolation_t interp);

/**
 * @brief Flip image
 * @param src Source image
 * @param dst Destination image (will be allocated)
 * @param mode Flip mode (horizontal/vertical/both)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_flip_image(const imgproc_image_buffer_t* src, imgproc_image_buffer_t* dst,
                                    imgproc_flip_mode_t mode);

/* ============================================================
 * Compression
 * ============================================================ */

typedef enum {
    IMGPROC_COMPRESS_NONE = 0,
    IMGPROC_COMPRESS_RLE,
    IMGPROC_COMPRESS_DELTA_ROW,
    IMGPROC_COMPRESS_JPEG
} imgproc_compress_type_t;

typedef struct {
    imgproc_compress_type_t type;
    uint8_t* data;
    size_t data_size;
    float ratio;
    double elapsed_ms;
} imgproc_compress_result_t;

/**
 * @brief Compress image data
 * @param img Source image
 * @param type Compression type
 * @param result Output result (must be freed with imgproc_compress_result_free)
 * @param jpeg_quality JPEG quality (1-100), only used for JPEG compression
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_compress(const imgproc_image_buffer_t* img, imgproc_compress_type_t type,
                                  imgproc_compress_result_t* result, int jpeg_quality);

/**
 * @brief Decompress image data
 * @param compressed Compressed data
 * @param compressed_size Compressed data size
 * @param type Compression type (must match the type used for compression)
 * @param width Original image width
 * @param height Original image height
 * @param format Original pixel format
 * @param img Output image (will be allocated)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_decompress(const uint8_t* compressed, size_t compressed_size,
                                    imgproc_compress_type_t type,
                                    int width, int height, imgproc_pixel_format_t format,
                                    imgproc_image_buffer_t* img);

/**
 * @brief Free compression result
 */
IMGPROC_API void imgproc_compress_result_free(imgproc_compress_result_t* result);

/* ============================================================
 * Barcode generation
 * ============================================================ */

typedef enum {
    IMGPROC_BARCODE_UNKNOWN = 0,
    IMGPROC_BARCODE_CODE128,
    IMGPROC_BARCODE_CODE39,
    IMGPROC_BARCODE_CODE93,
    IMGPROC_BARCODE_EAN13,
    IMGPROC_BARCODE_EAN8,
    IMGPROC_BARCODE_UPCA,
    IMGPROC_BARCODE_UPCE,
    IMGPROC_BARCODE_ITF,
    IMGPROC_BARCODE_CODABAR,
    IMGPROC_BARCODE_QRCODE,
    IMGPROC_BARCODE_DATAMATRIX,
    IMGPROC_BARCODE_PDF417,
    IMGPROC_BARCODE_AZTEC,
    IMGPROC_BARCODE_MAXICODE
} imgproc_barcode_type_t;

typedef struct {
    const char* text;
    imgproc_barcode_type_t type;
    int width;
    int height;
    int margin;
    uint32_t fg_color;
    uint32_t bg_color;
    int show_text;
    int font_size;
} imgproc_barcode_options_t;

/**
 * @brief Generate barcode to file
 * @param opts Barcode generation options
 * @param path Output file path (format by extension: .bmp, .png, .jpg)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_barcode_generate_file(const imgproc_barcode_options_t* opts, const char* path);

/**
 * @brief Generate barcode to image buffer
 * @param opts Barcode generation options
 * @param img Output image buffer (must be freed with imgproc_image_free)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_barcode_generate_image(const imgproc_barcode_options_t* opts, imgproc_image_buffer_t* img);

/**
 * @brief Generate barcode to memory
 * @param opts Barcode generation options
 * @param format Output format
 * @param out_data Receives pointer to allocated buffer (must be freed with free())
 * @param out_size Receives buffer size
 * @param jpeg_quality JPEG quality (1-100), ignored for other formats
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_barcode_generate_memory(const imgproc_barcode_options_t* opts,
                                                 imgproc_image_type_t format,
                                                 uint8_t** out_data, size_t* out_size,
                                                 int jpeg_quality);

/* ============================================================
 * Barcode reading
 * ============================================================ */

typedef struct {
    int success;
    char* text;
    imgproc_barcode_type_t type;
    const char* type_name;
} imgproc_barcode_result_t;

/**
 * @brief Read barcode from image file
 * @param path Image file path
 * @param result Output result (must be freed with imgproc_barcode_result_free)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_barcode_read_file(const char* path, imgproc_barcode_result_t* result);

/**
 * @brief Read barcode from memory buffer
 * @param data Image data
 * @param size Image data size
 * @param type Image type
 * @param result Output result (must be freed with imgproc_barcode_result_free)
 * @return 0 on success, non-zero on error
 */
IMGPROC_API int imgproc_barcode_read_memory(const uint8_t* data, size_t size,
                                             imgproc_image_type_t type,
                                             imgproc_barcode_result_t* result);

/**
 * @brief Free barcode result
 */
IMGPROC_API void imgproc_barcode_result_free(imgproc_barcode_result_t* result);

/* ============================================================
 * Utility functions
 * ============================================================ */

/**
 * @brief Detect image type from file extension
 */
IMGPROC_API imgproc_image_type_t imgproc_detect_type_by_extension(const char* path);

/**
 * @brief Detect image type from file content
 */
IMGPROC_API imgproc_image_type_t imgproc_detect_type_from_data(const uint8_t* data, size_t size);

/**
 * @brief Get library version string
 */
IMGPROC_API const char* imgproc_version(void);

/**
 * @brief Get last error message
 */
IMGPROC_API const char* imgproc_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* IMGPROC_C_API_H */
