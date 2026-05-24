#include "imgproc/imgproc_lua.hpp"
#include "imgproc/imgproc.hpp"
#include "imgproc/image_transform.hpp"

#include <lua.hpp>
#include <cstring>
#include <vector>
#include <string>

// 辅助宏：检查参数数量
#define CHECK_ARGS(L, n, name) \
    if (lua_gettop(L) != n) { \
        return luaL_error(L, "%s: expected %d arguments, got %d", name, n, lua_gettop(L)); \
    }

// 辅助函数：将 Lua 表转换为 QRCodeGenerateOptions
static bool luaToQRCodeOptions(lua_State* L, int idx, imgproc::QRCodeGenerateOptions& opts) {
    if (!lua_istable(L, idx)) return false;

    lua_getfield(L, idx, "text");
    if (lua_isstring(L, -1)) opts.text = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "width");
    if (lua_isnumber(L, -1)) opts.width = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "height");
    if (lua_isnumber(L, -1)) opts.height = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "margin");
    if (lua_isnumber(L, -1)) opts.margin = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "eccLevel");
    if (lua_isnumber(L, -1)) opts.eccLevel = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "fgColor");
    if (lua_isnumber(L, -1)) opts.fgColor = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "bgColor");
    if (lua_isnumber(L, -1)) opts.bgColor = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    return true;
}

// ============================================================
// 二维码生成
// ============================================================

static int l_qrcode_generate_file(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int size = luaL_optinteger(L, 3, 256);
    int ecc = luaL_optinteger(L, 4, 1);

    imgproc::QRCodeGenerateOptions opts;
    opts.text = text;
    opts.width = size;
    opts.height = size;
    opts.eccLevel = ecc;

    bool result = imgproc::generateQRCodeToFile(opts, path);
    lua_pushboolean(L, result);
    return 1;
}

static int l_qrcode_generate_memory(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const char* format = luaL_optstring(L, 2, "png");
    int size = luaL_optinteger(L, 3, 256);
    int ecc = luaL_optinteger(L, 4, 1);

    imgproc::QRCodeGenerateOptions opts;
    opts.text = text;
    opts.width = size;
    opts.height = size;
    opts.eccLevel = ecc;

    imgproc::ImageType imgType = imgproc::ImageType::PNG;
    if (strcmp(format, "jpeg") == 0 || strcmp(format, "jpg") == 0) {
        imgType = imgproc::ImageType::JPEG;
    } else if (strcmp(format, "bmp") == 0) {
        imgType = imgproc::ImageType::BMP;
    }

    std::vector<uint8_t> data;
    bool result = imgproc::generateQRCodeToMemory(opts, data, imgType);

    if (result) {
        lua_pushlstring(L, reinterpret_cast<const char*>(data.data()), data.size());
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to generate QR code");
        return 2;
    }
}

static int l_qrcode_read_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    auto result = imgproc::readQRCode(path);

    if (result.success) {
        lua_newtable(L);
        lua_pushstring(L, result.text.c_str());
        lua_setfield(L, -2, "text");
        lua_pushinteger(L, result.qrVersion);
        lua_setfield(L, -2, "version");
        lua_pushinteger(L, result.errorCorrectionLevel);
        lua_setfield(L, -2, "eccLevel");
        return 1;
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to read QR code");
        return 2;
    }
}

// ============================================================
// 图像变换
// ============================================================

static int l_image_resize(lua_State* L) {
    // 参数: input_path, output_path, new_width, new_height
    const char* inputPath = luaL_checkstring(L, 1);
    const char* outputPath = luaL_checkstring(L, 2);
    int newWidth = luaL_checkinteger(L, 3);
    int newHeight = luaL_checkinteger(L, 4);
    const char* interpStr = luaL_optstring(L, 5, "bilinear");

    imgproc::Interpolation interp = imgproc::Interpolation::Bilinear;
    if (strcmp(interpStr, "nearest") == 0) {
        interp = imgproc::Interpolation::Nearest;
    } else if (strcmp(interpStr, "bicubic") == 0) {
        interp = imgproc::Interpolation::Bicubic;
    }

    // 加载图像
    imgproc::ImageBuffer src;
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->loadFromFile(inputPath, src)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to load image");
            return 2;
        }
    }

    // 缩放
    imgproc::ImageBuffer dst;
    if (!imgproc::resizeImage(src, dst, newWidth, newHeight, interp)) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Failed to resize image");
        return 2;
    }

    // 保存
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->saveToFile(dst, outputPath)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to save image");
            return 2;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_image_crop(lua_State* L) {
    // 参数: input_path, output_path, x, y, width, height
    const char* inputPath = luaL_checkstring(L, 1);
    const char* outputPath = luaL_checkstring(L, 2);
    int x = luaL_checkinteger(L, 3);
    int y = luaL_checkinteger(L, 4);
    int width = luaL_checkinteger(L, 5);
    int height = luaL_checkinteger(L, 6);

    // 加载图像
    imgproc::ImageBuffer src;
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->loadFromFile(inputPath, src)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to load image");
            return 2;
        }
    }

    // 裁剪
    imgproc::ImageBuffer dst;
    if (!imgproc::cropImage(src, dst, x, y, width, height)) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Failed to crop image");
        return 2;
    }

    // 保存
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->saveToFile(dst, outputPath)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to save image");
            return 2;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_image_rotate(lua_State* L) {
    // 参数: input_path, output_path, angle, [expand]
    const char* inputPath = luaL_checkstring(L, 1);
    const char* outputPath = luaL_checkstring(L, 2);
    double angle = luaL_checknumber(L, 3);
    bool expand = lua_toboolean(L, 4);

    // 加载图像
    imgproc::ImageBuffer src;
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->loadFromFile(inputPath, src)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to load image");
            return 2;
        }
    }

    // 旋转
    imgproc::ImageBuffer dst;
    if (!imgproc::rotateImage(src, dst, angle, expand)) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Failed to rotate image");
        return 2;
    }

    // 保存
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->saveToFile(dst, outputPath)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to save image");
            return 2;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

static int l_image_flip(lua_State* L) {
    // 参数: input_path, output_path, mode ("h", "v", "both")
    const char* inputPath = luaL_checkstring(L, 1);
    const char* outputPath = luaL_checkstring(L, 2);
    const char* modeStr = luaL_checkstring(L, 3);

    imgproc::FlipMode mode = imgproc::FlipMode::Horizontal;
    if (strcmp(modeStr, "v") == 0 || strcmp(modeStr, "vertical") == 0) {
        mode = imgproc::FlipMode::Vertical;
    } else if (strcmp(modeStr, "both") == 0) {
        mode = imgproc::FlipMode::Both;
    }

    // 加载图像
    imgproc::ImageBuffer src;
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->loadFromFile(inputPath, src)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to load image");
            return 2;
        }
    }

    // 翻转
    imgproc::ImageBuffer dst;
    if (!imgproc::flipImage(src, dst, mode)) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Failed to flip image");
        return 2;
    }

    // 保存
    {
        auto codec = imgproc::createCrossCodec();
        if (!codec || !codec->saveToFile(dst, outputPath)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to save image");
            return 2;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

// ============================================================
// 文字渲染
// ============================================================

static int l_text_render(lua_State* L) {
    // 参数: text, output_path, [font_size], [font_path]
    const char* text = luaL_checkstring(L, 1);
    const char* outputPath = luaL_checkstring(L, 2);
    int fontSize = luaL_optinteger(L, 3, 24);
    const char* fontPath = luaL_optstring(L, 4, "");

    imgproc::TextRenderOptions opts;
    opts.text = text;
    opts.fontSize = fontSize;
    opts.fontPath = fontPath ? fontPath : "";

    bool result = imgproc::renderTextToFile(opts, outputPath);
    lua_pushboolean(L, result);
    return 1;
}

// ============================================================
// 注册函数
// ============================================================

static const luaL_Reg imgproc_funcs[] = {
    // 二维码
    {"qrcode_generate_file", l_qrcode_generate_file},
    {"qrcode_generate_memory", l_qrcode_generate_memory},
    {"qrcode_read_file", l_qrcode_read_file},

    // 图像变换
    {"image_resize", l_image_resize},
    {"image_crop", l_image_crop},
    {"image_rotate", l_image_rotate},
    {"image_flip", l_image_flip},

    // 文字渲染
    {"text_render", l_text_render},

    {nullptr, nullptr}
};

extern "C" int luaopen_imgproc(lua_State* L) {
    luaL_newlib(L, imgproc_funcs);

    // 添加常量
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "ECC_LOW");
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "ECC_MEDIUM");
    lua_pushinteger(L, 2);
    lua_setfield(L, -2, "ECC_QUARTILE");
    lua_pushinteger(L, 3);
    lua_setfield(L, -2, "ECC_HIGH");

    lua_pushstring(L, "1.0.0");
    lua_setfield(L, -2, "_VERSION");

    return 1;
}

namespace imgproc {
namespace lua {

bool registerAll(void* L) {
    luaopen_imgproc(static_cast<lua_State*>(L));
    return true;
}

} // namespace lua
} // namespace imgproc
