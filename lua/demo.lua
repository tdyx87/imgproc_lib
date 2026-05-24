#!/usr/bin/env lua
--[[
    imgproc_lib Lua Demo
    
    展示 imgproc 库的各种功能:
    - 二维码生成与读取
    - 图像变换 (缩放、裁剪、旋转、翻转)
    - 文字渲染
    
    运行方式:
    lua demo.lua [输出目录]
]]

local imgproc = require("imgproc")

-- 输出目录
local output_dir = arg[1] or "."
local function p(...) return table.concat({...}, "/") end

-- 辅助函数
local function ensure_dir(dir)
    -- 简单实现，假设目录已存在
end

local function print_header(title)
    print("")
    print("========================================")
    print(" " .. title)
    print("========================================")
end

local function print_section(title)
    print("")
    print("--- " .. title .. " ---")
end

-- 主程序
print_header("imgproc_lib Lua Demo")
print("版本: " .. (imgproc._VERSION or "unknown"))
print("输出目录: " .. output_dir)

ensure_dir(output_dir)

-- ============================================================
-- 1. 二维码生成
-- ============================================================
print_header("1. 二维码生成")

print_section("1.1 基础二维码")

-- 生成不同格式的二维码
local texts = {
    {text = "Hello World", file = "demo_qr_en.png", desc = "英文"},
    {text = "你好世界", file = "demo_qr_cn.png", desc = "中文"},
    {text = "https://www.example.com", file = "demo_qr_url.png", desc = "URL"},
    {text = "mailto:test@example.com", file = "demo_qr_email.png", desc = "邮箱"},
}

for _, item in ipairs(texts) do
    local path = p(output_dir, item.file)
    local ok = imgproc.qrcode_generate_file(item.text, path, 256, imgproc.ECC_MEDIUM)
    if ok then
        print(string.format("  [%s] %s -> %s", item.desc, item.text, item.file))
    else
        print(string.format("  [失败] %s", item.text))
    end
end

print_section("1.2 不同纠错等级")

local ecc_levels = {
    {level = imgproc.ECC_LOW, name = "L (7%)", file = "demo_qr_ecc_l.png"},
    {level = imgproc.ECC_MEDIUM, name = "M (15%)", file = "demo_qr_ecc_m.png"},
    {level = imgproc.ECC_QUARTILE, name = "Q (25%)", file = "demo_qr_ecc_q.png"},
    {level = imgproc.ECC_HIGH, name = "H (30%)", file = "demo_qr_ecc_h.png"},
}

for _, item in ipairs(ecc_levels) do
    local path = p(output_dir, item.file)
    local ok = imgproc.qrcode_generate_file("ECC Test", path, 256, item.level)
    if ok then
        print(string.format("  %s -> %s", item.name, item.file))
    end
end

print_section("1.3 不同输出格式")

local formats = {
    {fmt = "png", file = "demo_qr_format.png"},
    {fmt = "jpeg", file = "demo_qr_format.jpg"},
    {fmt = "bmp", file = "demo_qr_format.bmp"},
}

for _, item in ipairs(formats) do
    local path = p(output_dir, item.file)
    local ok = imgproc.qrcode_generate_file("Format Test", path, 256, imgproc.ECC_MEDIUM)
    if ok then
        print(string.format("  %s -> %s", item.fmt:upper(), item.file))
    end
end

print_section("1.4 内存生成")

-- 生成到内存并保存
local data, err = imgproc.qrcode_generate_memory("Memory QR", "png", 256)
if data then
    local path = p(output_dir, "demo_qr_memory.png")
    local f = io.open(path, "wb")
    if f then
        f:write(data)
        f:close()
        print(string.format("  内存生成成功，大小: %d 字节", #data))
        print("  已保存到: demo_qr_memory.png")
    end
else
    print("  内存生成失败: " .. tostring(err))
end

-- ============================================================
-- 2. 二维码读取
-- ============================================================
print_header("2. 二维码读取")

print_section("2.1 读取并验证")

local test_files = {
    "demo_qr_en.png",
    "demo_qr_cn.png",
    "demo_qr_url.png",
}

for _, file in ipairs(test_files) do
    local path = p(output_dir, file)
    local result = imgproc.qrcode_read_file(path)
    if result then
        print(string.format("  %s: \"%s\" (版本 %d)", file, result.text, result.version))
    else
        print(string.format("  %s: 读取失败", file))
    end
end

print_section("2.2 读取不同格式")

local format_files = {"demo_qr_format.png", "demo_qr_format.jpg", "demo_qr_format.bmp"}
for _, file in ipairs(format_files) do
    local path = p(output_dir, file)
    local result = imgproc.qrcode_read_file(path)
    if result then
        print(string.format("  %s: OK", file))
    else
        print(string.format("  %s: 失败", file))
    end
end

-- ============================================================
-- 3. 图像变换
-- ============================================================
print_header("3. 图像变换")

-- 准备源图像
local src_image = p(output_dir, "demo_qr_en.png")

print_section("3.1 图像缩放")

local resize_ops = {
    {w = 128, h = 128, file = "demo_resize_small.png", desc = "缩小"},
    {w = 512, h = 512, file = "demo_resize_large.png", desc = "放大"},
    {w = 300, h = 200, file = "demo_resize_diff.png", desc = "非等比"},
}

for _, op in ipairs(resize_ops) do
    local path = p(output_dir, op.file)
    local ok = imgproc.image_resize(src_image, path, op.w, op.h, "bilinear")
    if ok then
        print(string.format("  %s %dx%d -> %s", op.desc, op.w, op.h, op.file))
    end
end

print_section("3.2 图像裁剪")

local crop_ops = {
    {x = 0, y = 0, w = 128, h = 128, file = "demo_crop_tl.png", desc = "左上角"},
    {x = 64, y = 64, w = 128, h = 128, file = "demo_crop_center.png", desc = "中心"},
    {x = 128, y = 128, w = 128, h = 128, file = "demo_crop_br.png", desc = "右下角"},
}

for _, op in ipairs(crop_ops) do
    local path = p(output_dir, op.file)
    local ok = imgproc.image_crop(src_image, path, op.x, op.y, op.w, op.h)
    if ok then
        print(string.format("  %s (%d,%d %dx%d) -> %s", op.desc, op.x, op.y, op.w, op.h, op.file))
    end
end

print_section("3.3 图像旋转")

local rotate_ops = {
    {angle = 90, file = "demo_rotate_90.png", desc = "90°"},
    {angle = 180, file = "demo_rotate_180.png", desc = "180°"},
    {angle = 270, file = "demo_rotate_270.png", desc = "270°"},
    {angle = 45, file = "demo_rotate_45.png", desc = "45°"},
}

for _, op in ipairs(rotate_ops) do
    local path = p(output_dir, op.file)
    local ok = imgproc.image_rotate(src_image, path, op.angle, true)
    if ok then
        print(string.format("  %s -> %s", op.desc, op.file))
    end
end

print_section("3.4 图像翻转")

local flip_ops = {
    {mode = "h", file = "demo_flip_h.png", desc = "水平"},
    {mode = "v", file = "demo_flip_v.png", desc = "垂直"},
    {mode = "both", file = "demo_flip_both.png", desc = "双向"},
}

for _, op in ipairs(flip_ops) do
    local path = p(output_dir, op.file)
    local ok = imgproc.image_flip(src_image, path, op.mode)
    if ok then
        print(string.format("  %s翻转 -> %s", op.desc, op.file))
    end
end

-- ============================================================
-- 4. 文字渲染
-- ============================================================
print_header("4. 文字渲染")

print_section("4.1 不同字号")

local font_sizes = {16, 24, 36, 48, 72}
for _, size in ipairs(font_sizes) do
    local path = p(output_dir, string.format("demo_text_%d.png", size))
    local ok = imgproc.text_render(string.format("字号 %d", size), path, size)
    if ok then
        print(string.format("  字号 %dpx -> demo_text_%d.png", size, size))
    end
end

print_section("4.2 多语言文字")

local lang_texts = {
    {text = "Hello World", file = "demo_text_en.png"},
    {text = "你好世界", file = "demo_text_cn.png"},
    {text = "こんにちは", file = "demo_text_jp.png"},
    {text = "안녕하세요", file = "demo_text_kr.png"},
}

for _, item in ipairs(lang_texts) do
    local path = p(output_dir, item.file)
    local ok = imgproc.text_render(item.text, path, 36)
    if ok then
        print(string.format("  \"%s\" -> %s", item.text, item.file))
    end
end

-- ============================================================
-- 5. 综合示例
-- ============================================================
print_header("5. 综合示例")

print_section("5.1 批量生成名片二维码")

local contacts = {
    {name = "张三", phone = "13800138001", email = "zhangsan@example.com"},
    {name = "李四", phone = "13800138002", email = "lisi@example.com"},
    {name = "王五", phone = "13800138003", email = "wangwu@example.com"},
}

for i, contact in ipairs(contacts) do
    local vcard = string.format(
        "BEGIN:VCARD\nVERSION:3.0\nN:%s\nTEL:%s\nEMAIL:%s\nEND:VCARD",
        contact.name, contact.phone, contact.email
    )
    local path = p(output_dir, string.format("demo_vcard_%d.png", i))
    local ok = imgproc.qrcode_generate_file(vcard, path, 256, imgproc.ECC_MEDIUM)
    if ok then
        print(string.format("  名片 %d: %s -> demo_vcard_%d.png", i, contact.name, i))
    end
end

print_section("5.2 WiFi 配置二维码")

local wifi_configs = {
    {ssid = "MyWiFi", password = "password123", file = "demo_wifi_wpa.png"},
    {ssid = "GuestWiFi", password = "guest456", file = "demo_wifi_guest.png"},
}

for _, wifi in ipairs(wifi_configs) do
    -- WiFi 配置格式: WIFI:T:WPA;S:<SSID>;P:<password>;;
    local wifi_str = string.format("WIFI:T:WPA;S:%s;P:%s;;", wifi.ssid, wifi.password)
    local path = p(output_dir, wifi.file)
    local ok = imgproc.qrcode_generate_file(wifi_str, path, 256, imgproc.ECC_HIGH)
    if ok then
        print(string.format("  WiFi: %s -> %s", wifi.ssid, wifi.file))
    end
end

-- ============================================================
-- 完成
-- ============================================================
print_header("Demo 完成")
print(string.format("所有输出文件已保存到: %s", output_dir))
print("")
print("生成的文件列表:")

-- 列出生成的文件
local files = {
    "demo_qr_en.png", "demo_qr_cn.png", "demo_qr_url.png", "demo_qr_email.png",
    "demo_qr_ecc_l.png", "demo_qr_ecc_m.png", "demo_qr_ecc_q.png", "demo_qr_ecc_h.png",
    "demo_qr_format.png", "demo_qr_format.jpg", "demo_qr_format.bmp",
    "demo_qr_memory.png",
    "demo_resize_small.png", "demo_resize_large.png", "demo_resize_diff.png",
    "demo_crop_tl.png", "demo_crop_center.png", "demo_crop_br.png",
    "demo_rotate_90.png", "demo_rotate_180.png", "demo_rotate_270.png", "demo_rotate_45.png",
    "demo_flip_h.png", "demo_flip_v.png", "demo_flip_both.png",
    "demo_text_16.png", "demo_text_24.png", "demo_text_36.png", "demo_text_48.png", "demo_text_72.png",
    "demo_text_en.png", "demo_text_cn.png", "demo_text_jp.png", "demo_text_kr.png",
    "demo_vcard_1.png", "demo_vcard_2.png", "demo_vcard_3.png",
    "demo_wifi_wpa.png", "demo_wifi_guest.png",
}

local count = 0
for _, file in ipairs(files) do
    local path = p(output_dir, file)
    local f = io.open(path, "rb")
    if f then
        f:close()
        count = count + 1
    end
end

print(string.format("共生成 %d 个文件", count))
