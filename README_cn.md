选择语言 | Select a language

[**English**](README.md)  
[**简体中文**](README_cn.md)

# Debug Text

基于 **SDL2** 的**极简轻量级调试文本（点阵字体）绘制库**，**支持UTF-8字符集**（包括中文）

## 优点
- 程序**无需预先加载任何字体文件**  
- **Header-Only设计**。只需引用DebugText.hpp**马上就可以编译**，**无需多余配置**
- **跨平台可用**

## 项目结构

```
├── src/
│   └── generator.cpp          # 字形文件生成器主程序
├── test/
│   └── loadtest.cpp            # 位图字体渲染测试程序
├── include/                    # 公共头文件
│   ├── SDLWindowStuff.hpp      # SDL 窗口封装 (Windows DWM 支持)
│   ├── AdvancedSDLWindow.hpp   # 高级 SDL 窗口 Win32 API 封装
│   ├── DebugText.hpp           # 位图字体渲染引擎
│   ├── decode.hpp              # UTF-8 解码
│   ├── fusionpixel_12px_mono.hpp # 内嵌 12px 位图字体数据
│   ├── GL_Commdlg.hpp          # Windows 通用对话框封装
│   ├── GL_DWMoperations.hpp    # DWM 圆角窗口偏好设置
│   ├── SDL_EventPlus.hpp       # SDL 事件轮询封装
│   ├── SmoothCpp.hpp           # 数值平滑 / EMA 滤波器
│   ├── StopWatch.hpp           # 高精度计时器
│   ├── Times.hpp               # 帧率控制 (Timer)
│   ├── TimeCounter.h           # 简易时间计数器
│   └── UTF8toWide.hpp          # UTF-8 与宽字符串转换
├── font/
│   └── fusion-pixel-12px-monospaced-zh_hans.ttf  # Fusion Pixel 中文字体
├── tools/
│   └── toarray.py              # 任意文件 → C++ 字节数组 (.hpp) 转换工具
├── SDL2-2.24.0/                # SDL2 开发库 (include / lib)
├── SDL2_ttf-2.20.1/            # SDL2_ttf 开发库 (include / lib)
├── Makefile                    # 构建脚本
└── .vscode/
    ├── launch.json             # VS Code 调试配置
    └── tasks.json              # VS Code 构建任务
```
那些多余的hpp库是给字库生成器和演示程序用的，实际使用只需要fusionpixel_12px_mono.hpp和DebugText.hpp

## 依赖

### 库本身：

- **编译器:** 任意g++都可
- **C++ 标准:** C++17及以上
- **图形库:** SDL2
- **链接库:** SDL2main、SDL2
- **平台:** SDL支持哪些平台本库就支持哪些平台

### 生成器和测试：

- **编译器:** MinGW-w64 (g++)
- **C++ 标准:** C++20
- **图形库:** SDL2、SDL2_ttf
- **链接库:** SDL2main、SDL2、SDL2_ttf、dwmapi、imm32、comctl32、comdlg32、ole32、uuid
- **平台:** 仅Windows

> SDL2 和 SDL2_ttf 的开发库已包含在仓库中（`SDL2-2.24.0/`、`SDL2_ttf-2.20.1/`），无需额外下载。

## 构建

```bash
# 编译所有目标
make

# 仅编译 generator
make generator

# 仅编译 loadtest
make loadtest

# 清理构建产物
make clean
```

构建产物输出到 `build/` 目录。

### VS Code 构建

- 按 `Ctrl+Shift+B` 选择构建任务
- 按 `F5` 自动编译并启动调试

## 程序说明

### generator — 字形文件生成器

读取 `font/fusion-pixel-12px-monospaced-zh_hans.ttf`，遍历 Unicode BMP 范围（U+0000 ~ U+FFFF）的所有码点，将其渲染为 1 位位图并打包输出到 `font.bin`。

**输出格式 (`font.bin`):**

| 偏移 | 大小 | 说明 |
|------|------|------|
| 0 | 4 字节 | 字形总数 (int32) |
| 4 | 字形数 × 4 字节 | 字形索引表 (int32 数组, 每项为对应字形在文件中的偏移量) |
| ... | 变长 | 字形数据，每项包含: 宽度(uint8)、高度(uint8)、位图压缩数据 |

位图压缩方式：每像素 1 位，按行排列，白底为 `0`、非白底为 `1`。

### loadtest — 位图字体渲染测试

使用内嵌的 12px 位图字体数据 (`fusionpixel_12px_mono.hpp`) 渲染文本，演示 `DebugText.hpp` 引擎的渲染效果。

### tools/toarray.py — 文件转 C++ 字节数组

将任意二进制文件转换为 C++ `const uint8_t` 数组定义，用于将资源文件嵌入代码中。

```bash
python tools/toarray.py
```

## 许可

- 本项目遵循MIT协议
- 字体 `fusion-pixel-12px-monospaced-zh_hans.ttf` 来源于 [Fusion Pixel](https://github.com/TakWolf/fusion-pixel-font)，基于 OFL 许可（参见 `font/LICENSE-OFL`）。

## 闲谈
本来是想使用STM32给液晶屏幕绘制文本，但是默认的库只支持英文，索性手动做了一个字库  
做完后突然发现这招可以用到平时的开发中，不用提前加载字体文件实在方便很多！  
于是这款库就诞生了

## 在ImGui里面使用
理论可以，但我不喜欢用ImGui所以有闲心的大佬可以自行写适配