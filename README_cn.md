选择语言 | Select a language

[**English**](README.md)  
[**简体中文**](README_cn.md)

# Debug Text

基于 **SDL3** 的**极简轻量级调试文本（点阵字体）绘制库**，**支持UTF-8字符集**（包括中文）

## 优点
- 程序**无需预先加载任何字体文件**  
- **Header-Only设计**。只需引用 `DebugText.hpp` 加上想用的字库 header **马上就可以编译**，**无需多余配置**
- **跨平台可用**
- 支持**多字库共存**，按 FontID（字符串）切换 8/10/12px 等不同尺寸

## 文档

- [**API 参考（v2.0）**](docs/API_cn.md)：所有公开函数、类型、宏、返回值约定与行为细节

## 项目结构

```
├── src/
│   └── generator/              # 一站式字库生成器（TTF → 字库 header）
│       ├── main.cpp            # CLI 入口（--all / --ttf ... / --preview）
│       ├── FontBuilder.*       # 栅格化 + v2 打包
│       ├── HeaderEmitter.*     # 生成 header（constexpr 数组 + 自动注册）
│       └── Preview.*           # 可选 SDL 预览窗口
├── test/
│   ├── loadtest.cpp            # 多字库渲染演示（1/2/3 切换字库）
│   └── odr_compat.cpp          # 第二个翻译单元，验证 ODR 与注册表共享
├── include/                    # 公共头文件（其余辅助头文件位于 ../RecentlyC++Programs）
│   ├── SDL_WindowIM.hpp        # SDL 窗口封装 (Windows DWM 支持)
│   ├── DebugText.hpp           # 位图字体渲染引擎（字库驱动，v2 格式）
│   ├── decode.hpp              # UTF-8 解码
│   ├── fonts/                  # 生成的字库 header（可直接提交到仓库）
│   │   ├── fusion_pixel_8px_mono_zh_hans.hpp
│   │   ├── fusion_pixel_10px_mono_zh_hans.hpp
│   │   └── fusion_pixel_12px_mono_zh_hans.hpp
│   ├── GL_Commdlg.hpp          # Windows 通用对话框封装
│   ├── GL_DWMoperations.hpp    # DWM 圆角窗口偏好设置
│   ├── SDL_EventPlus.hpp       # SDL 事件轮询封装
│   ├── SmoothCpp.hpp           # 数值平滑 / EMA 滤波器
│   ├── StopWatch.hpp           # 高精度计时器
│   ├── Times.hpp               # 帧率控制 (Timer)
│   ├── TimeCounter.h           # 简易时间计数器
│   └── UTF8toWide.hpp          # UTF-8 与宽字符串转换
├── font/
│   └── fusion-pixel-font-12px-monospaced-ttf-v2026.10.25/  # Fusion Pixel 中文字体（含 OFL）
├── ../RecentlyC++Programs/     # SDL3 / SDL3_ttf 开发库与辅助头文件
├── CMakeLists.txt              # CMake 构建脚本
├── CMakePresets.json           # CMake 预设 (debug / release)
├── docs/
│   └── API_cn.md               # API 参考文档
└── .vscode/
    ├── launch.json             # VS Code 调试配置
    └── tasks.json              # VS Code 构建任务
```
那些多余的 hpp 库是给字库生成器和演示程序用的，实际使用只需要 `DebugText.hpp` 加上想引用的字库 header（`include/fonts/*.hpp`）。

## 依赖

### 库本身：

- **编译器:** 任意g++都可
- **C++ 标准:** C++17及以上
- **图形库:** SDL3
- **链接库:** SDL3
- **平台:** SDL支持哪些平台本库就支持哪些平台

### 生成器和测试：

- **编译器:** MinGW-w64 (g++)
- **C++ 标准:** C++20
- **图形库:** SDL3、SDL3_ttf
- **链接库:** SDL3、SDL3_ttf、dwmapi、imm32、comctl32、comdlg32、shell32、gdi32、ole32、uuid
- **平台:** 仅Windows

> SDL3 和 SDL3_ttf 的开发库位于同级目录 `../RecentlyC++Programs/`（`SDL3-3.4.14/`、`SDL3_ttf-3.2.2/`），无需额外下载。

## 构建

本工程使用 CMake + CMakePresets 构建（需要 CMake 3.21+、Ninja 与 MinGW-w64 g++）。

```bash
# 配置并编译 Debug 版本（输出到 bin/debug/）
cmake --preset debug
cmake --build --preset debug

# 配置并编译 Release 版本（输出到 bin/release/）
cmake --preset release
cmake --build --preset release

# 仅编译单个目标
cmake --build --preset debug --target generator
cmake --build --preset debug --target loadtest

# 重新生成字库 header（输出到 include/fonts/）
cmake --build --preset debug --target fonts

# 清理构建产物
cmake --build --preset debug --target clean
```

构建产物输出到 `bin/debug/` 或 `bin/release/`，SDL3 运行库 DLL 会自动复制到可执行文件旁边。
`generator` 是控制台程序，默认从仓库根目录的 `font/` 下找预设字体；不在仓库根目录运行时用 `--root` 指定。

### VS Code 构建

- 按 `Ctrl+Shift+B` 选择构建任务
- 按 `F5` 自动编译并启动调试

## 程序说明

### generator — 一站式字库生成器

直接读取 TTF 并把点阵数据写成 C++ 字库 header（不再产出中间 `font.bin`，也不需要额外的转换脚本）。

```bash
# 重新生成全部预设（8/10/12px mono zh_hans）→ include/fonts/*.hpp
cmake --build --preset debug --target fonts

# 也可以直接调用 CLI
bin/debug/generator.exe --all --root .
bin/debug/generator.exe --ttf path/to/font.ttf --id my-font --out include/fonts/my_font.hpp --preview
```

- 用 `TTF_FontHasGlyph()` 自动探测字体覆盖到的最高码点（含 CJK 扩展区等辅助平面字形）
- 位图归一化到 `cellWidth × cellHeight`，半角/全角由 advance 决定；少数超格字形写入 extras 侧表
- 输出确定（无时间戳），内容未变化时不会改写文件，避免无谓的重新编译
- `--preview` 会开一个 SDL 预览窗口（NEAREST 缩放），实时显示当前字形与进度

**v2 字库格式（全部为 constexpr 常量数组）**

| 段 | 说明 |
|----|------|
| `presenceBits` + `presenceBlockCounts` | 码点 → 字形序号（位图 + 每 256 码点累计计数，popcount 得到序号） |
| `fullWidthBits` + `fullWidthBlockCounts` | 字形序号 → 半角/全角（1 bit/字形） |
| `atlas` | 位图按码点升序紧排，记录长度 = ceil(w*h/8)（12px 为例：半角 9B / 全角 18B） |
| `extra*` | 高度/宽度超出单元格的字形侧表 |

位意义：**1 = 墨点**。相比旧的 `font.bin` 格式，去掉了 65536 项的 int32 偏移表和每个字形 2 字节的记录头，实测三个字库体积分别下降 **75.7% / 75.1% / 62.1%**（8/10/12px）。

### loadtest — 多字库渲染测试

同一帧内按 FontID 分别绘制 8/10/12px 三个字库；按 `1`/`2`/`3` 或 `Tab` 切换当前字库，`↑`/`↓` 调整缩放，`Esc` 退出。

## 使用示例

```cpp
#include "DebugText.hpp"
#include "fonts/fusion_pixel_10px_mono_zh_hans.hpp"
#include "fonts/fusion_pixel_12px_mono_zh_hans.hpp"

// 1) 设置当前字库，之后 paintDBText* 都使用它
dbtext::setFont("fusion-pixel-12px-mono-zh_hans");
paintDBTextFormat(renderer, 10, 10, {0, 0, 0, 255}, 2, {0, 0}, "FPS: %.0f", fps);

// 2) 按 FontID 指定字库
dbtext::paintTextFormat(renderer, dbtext::FontID("fusion-pixel-10px-mono-zh_hans"),
                        10, 40, {0, 0, 0, 255}, 3, {0, 0}, "小号字库");

// 2b) 前景色 + 背景色（背景 alpha = 0 表示透明，也可直接省略）
dbtext::paintTextFormat(renderer, dbtext::FontID("fusion-pixel-10px-mono-zh_hans"),
                        10, 60, {0, 0, 0, 255}, {255, 236, 179, 255}, 3, {0, 0}, "带背景的字库");

// 3) RAII 临时切换
{
    dbtext::FontScope scope("fusion-pixel-10px-mono-zh_hans");
    paintDBText(renderer, 10, 80, {0, 0, 0, 255}, 2, "临时使用 10px");
}

// 4) 只量尺寸 / 需要 SDL_Surface
dbtext::TextSize size = dbtext::measureText(*dbtext::currentFont(), "Hello 中文");
SDL_Surface* surface = dbtext::createTextSurface(dbtext::currentFont(), "Hello", {0, 0, 0, 255});
```

字库 header 在被包含时会自动注册（`dbtext::AutoRegister`），因此同时 include 多个字库即可按要求切换；未调用 `setFont` 时默认使用 `DBTEXT_DEFAULT_FONT_ID`（默认为 12px 那一档，可用宏覆盖）。

## 许可

- 本项目遵循MIT协议
- 字体 `fusion-pixel-*-monospaced-zh_hans.ttf` 来源于 [Fusion Pixel](https://github.com/TakWolf/fusion-pixel-font)，基于 OFL 许可（参见 `font/fusion-pixel-font-*/OFL.txt` 与各 `LICENSES/` 子目录）。
- 生成的点阵字库属于该字体的派生数据，再导出为 TTF 时请勿沿用保留字体名（内置导出名固定为 `DebugBitmapFont`）。

## 闲谈
本来是想使用STM32给液晶屏幕绘制文本，但是默认的库只支持英文，索性手动做了一个字库  
做完后突然发现这招可以用到平时的开发中，不用提前加载字体文件实在方便很多！  
于是这款库就诞生了

## 在ImGui里面使用
理论可以，但我不喜欢用ImGui所以有闲心的大佬可以自行写适配