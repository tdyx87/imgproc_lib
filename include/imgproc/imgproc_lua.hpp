#pragma once

// Lua C API 绑定 for imgproc_lib
// 需要链接 lua5.x.lib

#ifdef __cplusplus
extern "C" {
#endif

// 将 imgproc_lib 注册到 Lua 虚拟机
// 在 lua_State 上创建 'imgproc' 表，包含所有功能
// 返回 1（压入栈的值数量）
int luaopen_imgproc(void* L);

#ifdef __cplusplus
}
#endif

// C++ 辅助函数：创建绑定（如果需要在 C++ 中手动注册）
namespace imgproc {
namespace lua {

    // 注册所有 imgproc 功能到指定的 Lua 状态
    bool registerAll(void* L);

} // namespace lua
} // namespace imgproc
