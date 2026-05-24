--[[
    imgproc_lib Lua 绑定单元测试
    
    运行方式:
    lua test_imgproc.lua [输出目录]
    
    依赖:
    - lua 5.1+ 或 luajit
    - imgproc 动态库 (imgproc.dll / imgproc.so)
]]

local imgproc = require("imgproc")
local os = os
local io = io
local string = string
local print = print
local assert = assert
local pcall = pcall

-- 测试结果统计
local passed = 0
local failed = 0
local test_output_dir = arg[1] or "."

-- 辅助函数
local function test(name, func)
    local ok, err = pcall(func)
    if ok then
        print("[PASS] " .. name)
        passed = passed + 1
    else
        print("[FAIL] " .. name .. ": " .. tostring(err))
        failed = failed + 1
    end
end

local function path_join(...)
    return table.concat({...}, "/")
end

local function file_exists(path)
    local f = io.open(path, "rb")
    if f then
        f:close()
        return true
    end
    return false
end

local function file_size(path)
    local f = io.open(path, "rb")
    if not f then return 0 end
    local size = f:seek("end")
    f:close()
    return size
end

-- ============================================================
-- 测试套件
-- ============================================================

print("========================================")
print("imgproc_lib Lua 绑定单元测试")
print("版本: " .. (imgproc._VERSION or "unknown"))
print("输出目录: " .. test_output_dir)
print("========================================")
print("")

-- ------------------------------------------------------------
-- 基础功能测试
-- ------------------------------------------------------------
print("--- 基础功能 ---")

test("库加载成功", function()
    assert(imgproc ~= nil, "imgproc 模块加载失败")
end)

test("版本号存在", function()
    assert(imgproc._VERSION ~= nil, "_VERSION 不存在")
end)

test("常量定义正确", function()
    assert(imgproc.ECC_LOW == 0, "ECC_LOW 应为 0")
    assert(imgproc.ECC_MEDIUM == 1, "ECC_MEDIUM 应为 1")
    assert(imgproc.ECC_QUARTILE == 2, "ECC_QUARTILE 应为 2")
    assert(imgproc.ECC_HIGH == 3, "ECC_HIGH 应为 3")
end)

-- ------------------------------------------------------------
-- 二维码生成测试
-- ------------------------------------------------------------
print("")
print("--- 二维码生成 ---")

test("生成二维码到文件 (PNG)", function()
    local path = path_join(test_output_dir, "test_qr_basic.png")
    local ok = imgproc.qrcode_generate_file("Hello Lua!", path, 256, imgproc.ECC_MEDIUM)
    assert(ok, "生成失败")
    assert(file_exists(path), "文件未创建")
    assert(file_size(path) > 0, "文件为空")
end)

test("生成二维码到文件 (BMP)", function()
    local path = path_join(test_output_dir, "test_qr_basic.bmp")
    local ok = imgproc.qrcode_generate_file("Hello BMP!", path, 256)
    assert(ok, "生成失败")
    assert(file_exists(path), "文件未创建")
end)

test("生成二维码到文件 (JPEG)", function()
    local path = path_join(test_output_dir, "test_qr_basic.jpg")
    local ok = imgproc.qrcode_generate_file("Hello JPEG!", path, 256, imgproc.ECC_HIGH)
    assert(ok, "生成失败")
    assert(file_exists(path), "文件未创建")
end)

test("生成二维码到内存 (PNG)", function()
    local data, err = imgproc.qrcode_generate_memory("Memory Test", "png", 256)
    assert(data ~= nil, "生成失败: " .. tostring(err))
    assert(#data > 0, "返回数据为空")
    -- PNG 签名检查
    local sig = data:sub(1, 8)
    assert(sig == "\137PNG\r\n\26\n", "不是有效的 PNG 数据")
end)

test("生成二维码到内存 (JPEG)", function()
    local data, err = imgproc.qrcode_generate_memory("JPEG Memory", "jpeg", 256)
    assert(data ~= nil, "生成失败: " .. tostring(err))
    assert(#data > 0, "返回数据为空")
    -- JPEG 签名检查
    local sig = data:sub(1, 2)
    assert(sig == "\255\216", "不是有效的 JPEG 数据")
end)

test("生成二维码到内存 (BMP)", function()
    local data, err = imgproc.qrcode_generate_memory("BMP Memory", "bmp", 256)
    assert(data ~= nil, "生成失败: " .. tostring(err))
    assert(#data > 0, "返回数据为空")
    -- BMP 签名检查
    local sig = data:sub(1, 2)
    assert(sig == "BM", "不是有效的 BMP 数据")
end)

test("中文二维码生成", function()
    local path = path_join(test_output_dir, "test_qr_chinese.png")
    local ok = imgproc.qrcode_generate_file("你好世界", path, 256)
    assert(ok, "生成失败")
    assert(file_exists(path), "文件未创建")
end)

test("长文本二维码生成", function()
    local long_text = string.rep("A", 100)
    local path = path_join(test_output_dir, "test_qr_long.png")
    local ok = imgproc.qrcode_generate_file(long_text, path)
    assert(ok, "生成失败")
end)

test("URL 二维码生成", function()
    local path = path_join(test_output_dir, "test_qr_url.png")
    local ok = imgproc.qrcode_generate_file("https://www.example.com/path?query=value", path, 256, imgproc.ECC_HIGH)
    assert(ok, "生成失败")
end)

-- ------------------------------------------------------------
-- 二维码读取测试
-- ------------------------------------------------------------
print("")
print("--- 二维码读取 ---")

test("读取二维码文件", function()
    local path = path_join(test_output_dir, "test_qr_basic.png")
    local result, err = imgproc.qrcode_read_file(path)
    assert(result ~= nil, "读取失败: " .. tostring(err))
    assert(result.text == "Hello Lua!", "内容不匹配: " .. tostring(result.text))
end)

test("读取中文二维码", function()
    local path = path_join(test_output_dir, "test_qr_chinese.png")
    local result, err = imgproc.qrcode_read_file(path)
    assert(result ~= nil, "读取失败: " .. tostring(err))
    assert(result.text == "你好世界", "内容不匹配: " .. tostring(result.text))
end)

test("读取 JPEG 二维码", function()
    local path = path_join(test_output_dir, "test_qr_basic.jpg")
    local result, err = imgproc.qrcode_read_file(path)
    assert(result ~= nil, "读取失败: " .. tostring(err))
    assert(result.text == "Hello JPEG!", "内容不匹配: " .. tostring(result.text))
end)

test("读取 BMP 二维码", function()
    local path = path_join(test_output_dir, "test_qr_basic.bmp")
    local result, err = imgproc.qrcode_read_file(path)
    assert(result ~= nil, "读取失败: " .. tostring(err))
    assert(result.text == "Hello BMP!", "内容不匹配: " .. tostring(result.text))
end)

test("读取返回版本信息", function()
    local path = path_join(test_output_dir, "test_qr_basic.png")
    local result = imgproc.qrcode_read_file(path)
    assert(result.version ~= nil, "缺少 version 字段")
    assert(result.eccLevel ~= nil, "缺少 eccLevel 字段")
end)

-- ------------------------------------------------------------
-- 图像变换测试
-- ------------------------------------------------------------
print("")
print("--- 图像变换 ---")

test("图像缩放", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_resize.png")
    local ok, err = imgproc.image_resize(input, output, 128, 128, "bilinear")
    assert(ok, "缩放失败: " .. tostring(err))
    assert(file_exists(output), "输出文件未创建")
end)

test("图像缩放 (最近邻)", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_resize_nearest.png")
    local ok = imgproc.image_resize(input, output, 512, 512, "nearest")
    assert(ok, "缩放失败")
end)

test("图像裁剪", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_crop.png")
    local ok, err = imgproc.image_crop(input, output, 50, 50, 100, 100)
    assert(ok, "裁剪失败: " .. tostring(err))
    assert(file_exists(output), "输出文件未创建")
end)

test("图像旋转 90 度", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_rotate_90.png")
    local ok = imgproc.image_rotate(input, output, 90, true)
    assert(ok, "旋转失败")
end)

test("图像旋转 45 度", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_rotate_45.png")
    local ok = imgproc.image_rotate(input, output, 45, true)
    assert(ok, "旋转失败")
end)

test("图像水平翻转", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_flip_h.png")
    local ok = imgproc.image_flip(input, output, "h")
    assert(ok, "翻转失败")
end)

test("图像垂直翻转", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_flip_v.png")
    local ok = imgproc.image_flip(input, output, "v")
    assert(ok, "翻转失败")
end)

test("图像双向翻转", function()
    local input = path_join(test_output_dir, "test_qr_basic.png")
    local output = path_join(test_output_dir, "test_flip_both.png")
    local ok = imgproc.image_flip(input, output, "both")
    assert(ok, "翻转失败")
end)

-- ------------------------------------------------------------
-- 文字渲染测试
-- ------------------------------------------------------------
print("")
print("--- 文字渲染 ---")

test("渲染英文文字", function()
    local output = path_join(test_output_dir, "test_text_en.png")
    local ok = imgproc.text_render("Hello World", output, 48)
    assert(ok, "渲染失败")
    assert(file_exists(output), "输出文件未创建")
end)

test("渲染中文文字", function()
    local output = path_join(test_output_dir, "test_text_cn.png")
    local ok = imgproc.text_render("你好世界", output, 36)
    assert(ok, "渲染失败")
end)

test("渲染混合文字", function()
    local output = path_join(test_output_dir, "test_text_mix.png")
    local ok = imgproc.text_render("Hello 世界", output, 32)
    assert(ok, "渲染失败")
end)

-- ------------------------------------------------------------
-- 边界条件测试
-- ------------------------------------------------------------
print("")
print("--- 边界条件 ---")

test("空文本二维码生成", function()
    local path = path_join(test_output_dir, "test_qr_empty.png")
    local ok = imgproc.qrcode_generate_file("", path, 128)
    -- 空文本可能失败，这是预期行为
    -- 不做断言，只检查不会崩溃
end)

test("读取不存在的文件", function()
    local result, err = imgproc.qrcode_read_file("nonexistent_file.png")
    assert(result == nil, "应该返回 nil")
end)

test("生成到无效路径", function()
    local ok = imgproc.qrcode_generate_file("Test", "/invalid/path/qr.png", 256)
    -- 应该失败但不崩溃
    assert(ok == false, "应该返回 false")
end)

-- ============================================================
-- 测试结果汇总
-- ============================================================
print("")
print("========================================")
print("测试结果汇总")
print("========================================")
print(string.format("通过: %d", passed))
print(string.format("失败: %d", failed))
print(string.format("总计: %d", passed + failed))
print("")

if failed > 0 then
    print("存在失败的测试!")
    os.exit(1)
else
    print("所有测试通过!")
    os.exit(0)
end
