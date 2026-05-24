# imgproc_lib Lua 绑定

## 构建

启用 Lua 绑定需要 CMake 配置时设置 `BUILD_LUA_BINDINGS=ON`：

```bash
cmake -S . -B build -DBUILD_LUA_BINDINGS=ON
cmake --build build --config Release
```

### 依赖

- Lua 5.1+ 或 LuaJIT
- CMake 会自动查找系统中的 Lua

## 安装

```bash
cmake --install build
```

Lua 脚本将安装到 `share/imgproc/lua/` 目录。

## 使用方法

### 加载模块

```lua
local imgproc = require("imgproc")
```

确保 `imgproc.dll` (Windows) 或 `imgproc.so` (Linux/macOS) 在 Lua 的搜索路径中。

### API 参考

#### 常量

```lua
imgproc._VERSION      -- 版本字符串
imgproc.ECC_LOW       -- 纠错等级 L (7%)
imgproc.ECC_MEDIUM    -- 纠错等级 M (15%)
imgproc.ECC_QUARTILE  -- 纠错等级 Q (25%)
imgproc.ECC_HIGH      -- 纠错等级 H (30%)
```

#### 二维码生成

```lua
-- 生成到文件
imgproc.qrcode_generate_file(text, path, [size], [ecc])
-- text: 要编码的文本
-- path: 输出文件路径 (格式由扩展名决定: .png/.jpg/.bmp)
-- size: 尺寸 (可选, 默认 256)
-- ecc: 纠错等级 (可选, 默认 ECC_MEDIUM)
-- 返回: true/false

-- 生成到内存
imgproc.qrcode_generate_memory(text, [format], [size], [ecc])
-- format: "png" / "jpeg" / "bmp" (可选, 默认 "png")
-- 返回: data, err (成功返回数据字符串, 失败返回 nil 和错误信息)
```

#### 二维码读取

```lua
imgproc.qrcode_read_file(path)
-- 返回: result, err
-- result.text: 解码内容
-- result.version: QR 版本
-- result.eccLevel: 纠错等级
```

#### 图像变换

```lua
-- 缩放
imgproc.image_resize(input_path, output_path, width, height, [interpolation])
-- interpolation: "nearest" / "bilinear" / "bicubic" (可选, 默认 "bilinear")

-- 裁剪
imgproc.image_crop(input_path, output_path, x, y, width, height)

-- 旋转
imgproc.image_rotate(input_path, output_path, angle, [expand])
-- angle: 角度 (逆时针)
-- expand: 是否扩展画布 (可选, 默认 true)

-- 翻转
imgproc.image_flip(input_path, output_path, mode)
-- mode: "h" (水平) / "v" (垂直) / "both" (双向)
```

#### 文字渲染

```lua
imgproc.text_render(text, output_path, [font_size], [font_path])
-- font_size: 字号 (可选, 默认 24)
-- font_path: 字体文件路径 (可选, 使用系统默认字体)
```

## 示例

### 生成二维码

```lua
local imgproc = require("imgproc")

-- 生成 PNG
imgproc.qrcode_generate_file("Hello World", "qr.png", 256, imgproc.ECC_HIGH)

-- 生成到内存
local data = imgproc.qrcode_generate_memory("Hello", "png", 256)
local file = io.open("output.png", "wb")
file:write(data)
file:close()
```

### 读取二维码

```lua
local result = imgproc.qrcode_read_file("qr.png")
if result then
    print("内容: " .. result.text)
    print("版本: " .. result.version)
end
```

### 图像处理

```lua
-- 缩放
imgproc.image_resize("input.png", "output.png", 800, 600, "bilinear")

-- 裁剪中心区域
imgproc.image_crop("input.png", "output.png", 100, 100, 200, 200)

-- 旋转 45 度
imgproc.image_rotate("input.png", "output.png", 45, true)

-- 水平翻转
imgproc.image_flip("input.png", "output.png", "h")
```

## 运行测试

```bash
# 运行单元测试
lua lua/test_imgproc.lua ./output

# 运行 demo
lua lua/demo.lua ./output
```

## 错误处理

大多数函数返回 `true/false` 或 `data/nil`，失败时可通过第二个返回值获取错误信息：

```lua
local ok, err = imgproc.qrcode_generate_file("Test", "/invalid/path.png")
if not ok then
    print("错误: " .. tostring(err))
end
```
