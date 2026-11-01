# DebugText API 参考（v2.0）

> 对应头文件：`include/DebugText.hpp`
> 字库 header：`include/fonts/*.hpp`（由 `generator` 生成）
> 命名空间：`dbtext`，TTF 导出部分在 `dbtext::ttf`
> 要求：C++17 及以上；链接 SDL3；使用 `dbtext::ttf` 时额外需要 SDL3_ttf

---

## 目录

1. [快速开始](#1-快速开始)
2. [配置宏](#2-配置宏)
3. [类型](#3-类型)
   - [3.1 FontID](#31-fontid)
   - [3.2 GlyphInfo](#32-glyphinfo)
   - [3.3 FontDesc](#33-fontdesc)
   - [3.4 TextSize](#34-textsize)
   - [3.5 AutoRegister](#35-autoregister)
   - [3.6 FontScope](#36-fontscope)
4. [常量](#4-常量)
5. [字库注册与查询](#5-字库注册与查询)
6. [字形查询](#6-字形查询)
7. [度量与渲染](#7-度量与渲染)
8. [旧版兼容接口](#8-旧版兼容接口)
9. [TTF 导出（可选）](#9-ttf-导出可选)
10. [解码辅助函数](#10-解码辅助函数)
11. [字库 header 结构与自写字库](#11-字库-header-结构与自写字库)
12. [行为细节与注意事项](#12-行为细节与注意事项)

---

## 1. 快速开始

```cpp
#include "DebugText.hpp"
#include "fonts/fusion_pixel_12px_mono_zh_hans.hpp"   // 包含即自动注册

// —— 方式 A：设置当前字库，之后所有旧接口都使用它 ——
dbtext::setFont("fusion-pixel-12px-mono-zh_hans");
paintDBTextFormat(renderer, 10, 10, {0, 0, 0, 255}, 2.0f, {0.0f, 0.0f}, "FPS: %.0f", fps);

// —— 方式 B：按 FontID 指定字库 ——
dbtext::paintText(renderer, "fusion-pixel-8px-mono-zh_hans",
                  10, 40, {0, 0, 0, 255}, 3.0f, "8px 文本");

// —— 方式 B2：前景色 + 背景色（背景 alpha = 0 即透明）——
dbtext::paintText(renderer, "fusion-pixel-8px-mono-zh_hans",
                  10, 60, {0, 0, 0, 255}, {255, 236, 179, 255}, 3.0f, "带背景的文本");

// —— 方式 C：RAII 临时切换 ——
{
    dbtext::FontScope scope("fusion-pixel-8px-mono-zh_hans");
    paintDBText(renderer, 10, 80, {0, 0, 0, 255}, 2.0f, "临时使用 8px");
}

// —— 只要尺寸 / 只要 SDL_Surface ——
dbtext::TextSize size = dbtext::measureText(*dbtext::currentFont(), "Hello 中文");
SDL_Surface* surface = dbtext::createTextSurface(dbtext::currentFont(), "Hello", {0, 0, 0, 255});
if (surface != nullptr) SDL_DestroySurface(surface);
```

要点：

- 字库 header 只包含数据与注册对象，**包含即注册**（`dbtext::AutoRegister`），可同时包含多个。
- `paintText*` / `createTextSurface` 都是**同步绘制**：每次调用都会新建 surface/texture，画完即销毁，调用方无需管理资源。
- 缩放参数统一是**倍数**（`1.0f` = 原始像素大小），不是磅值。

---

## 2. 配置宏

以下宏需要在 **包含 `DebugText.hpp` 之前** 定义（通常用 `-D` 或写在包含语句上方）。

| 宏 | 默认值 | 说明 |
|---|---|---|
| `DBTEXT_DEFAULT_FONT_ID` | `"fusion-pixel-12px-mono-zh_hans"` | 未调用 `setFont()` 时使用哪个字库。若该 ID 未注册，回退到「第一个注册的字库」 |
| `DBTEXT_FALLBACK_CODEPOINT` | `0xFFFD`（U+FFFD） | 缺字时的第一回退码点，依次回退到 `'?'` 与 `' '` |
| `DBTEXT_NO_LEGACY_API` | 未定义 | 定义后不再提供 `createDBTextSurface` / `paintDBText` / `paintDBTextFormat` |
| `DBTEXT_ENABLE_TTF_GENERATION` | 未定义 | 定义后启用 `dbtext::ttf`（点阵字库 → 内存 TTF，需要 SDL3_ttf） |

---

## 3. 类型

### 3.1 `FontID`

```cpp
using FontID = std::string_view;
```

字库的唯一标识字符串，例如 `"fusion-pixel-12px-mono-zh_hans"`。

- 由字库 header 的 `FontDesc::id` 提供，指向**静态存储期**的字符串常量。
- 查找只做字符串比较，不区分大小写**不支持**（严格逐字节比较）。
- 所有接受 `FontID` 的接口也接受 `const char*` 字面量和 `std::string`（隐式转换）。

### 3.2 `GlyphInfo`

单个字形的只读视图，由 `FontDesc::findGlyph()` 填充。

| 成员 | 类型 | 说明 |
|---|---|---|
| `width` | `uint16_t` | 位图宽度（像素），**同时也是前进量 advance** |
| `height` | `uint16_t` | 位图高度（像素） |
| `words` | `const uint32_t*` | 位图所在字节流的 32 位词视图 |
| `byteOffset` | `uint32_t` | 本字形位图在该字节流中的起始字节偏移 |

成员函数：

| 函数 | 说明 |
|---|---|
| `uint8_t byteAt(uint32_t index) const noexcept` | 读取位图字节流中 `byteOffset + index` 处的字节 |
| `bool bitAt(uint32_t index) const noexcept` | 读取第 `index` 位（**按行紧密排列**：`index = y * width + x`），返回 `true` 表示墨点 |

> 位图**不是**按字节对齐到每行末尾的，而是整个字形按位连续打包；用 `bitAt(y * width + x)` 取点即可。

### 3.3 `FontDesc`

字库描述结构体。由 generator 生成的字库 header 以 `constexpr` 常量形式提供；内含全部数据指针，本身可自由复制（但注册表保存的是**指针**，因此 `FontDesc` 实例必须有静态存储期）。

#### 头部信息

| 字段 | 类型 | 说明 |
|---|---|---|
| `formatVersion` | `uint16_t` | 字库格式版本，当前为 `2`（`dbtext::kFontFormatVersion`） |
| `flags` | `uint16_t` | bit0 = 1 表示位图中 `1` 为墨点 |
| `id` | `const char*` | FontID（唯一） |
| `displayName` | `const char*` | 人类可读名称，如 `Fusion Pixel 12px Mono zh-Hans` |
| `styleName` | `const char*` | 风格，如 `Regular` |
| `copyright` | `const char*` | 版权声明 |
| `licenseName` | `const char*` | 许可名称，如 `SIL Open Font License 1.1` |
| `sourceFile` | `const char*` | 源字体文件名 |
| `sourceVersion` | `const char*` | 源字体版本 |

#### 度量

| 字段 | 类型 | 说明 |
|---|---|---|
| `cellWidth` | `uint8_t` | 全角字宽（像素） |
| `cellHeight` | `uint8_t` | 位图高，同时是**默认行高**（像素） |
| `halfWidth` | `uint8_t` | 半角字宽（像素） |
| `tabWidth` | `uint8_t` | `\t` 的步进（像素），默认等于 4 个全角宽 |

#### 数据段（一般无需直接访问）

| 字段 | 说明 |
|---|---|
| `maxCodepoint` / `glyphCount` | 最大码点 / 字形总数 |
| `presenceWordCount` / `presenceBits` / `presenceBlockCounts` | 码点 → 字形序号的位图索引 |
| `fullWidthWordCount` / `fullWidthBits` / `fullWidthBlockCounts` | 字形序号 → 全角/半角的位图索引 |
| `halfRecordBytes` / `fullRecordBytes` | 半角/全角字形在 atlas 中的记录字节数 |
| `atlas` | 位图字节流（按码点升序紧密排列） |
| `extraCount` / `extraBits` / `extraBlockCounts` / `extraDims` / `extraOffsets` / `extraData` | 超格字形（宽或高超出单元格）的侧表 |

#### 成员函数

| 函数 | 返回值 | 说明 |
|---|---|---|
| `bool valid() const noexcept` | 是否可用 | 校验版本号与必需的数据指针，注册与渲染前都会检查 |
| `uint32_t glyphIndex(uint32_t codepoint) const noexcept` | 字形序号，或 `kNoGlyphIndex` | 码点 → 字形序号（O(1)） |
| `uint32_t fullWidthBefore(uint32_t index) const noexcept` | 数量 | 字形序号 `index` 之前的全角字形数 |
| `bool isFullWidth(uint32_t index) const noexcept` | 是否全角 | 字形序号 → 全角/半角 |
| `bool isExtra(uint32_t index) const noexcept` | 是否超格 | 该字形是否存放在 extras 侧表 |
| `bool findGlyph(uint32_t codepoint, GlyphInfo& out) const noexcept` | 是否找到 | 查询码点对应的字形；找到时填充 `out` |
| `bool findGlyph(uint32_t codepoint, GlyphInfo& out, uint32_t fallbackCodepoint) const noexcept` | 是否找到 | 先查 `codepoint`，失败再查 `fallbackCodepoint` |

### 3.4 `TextSize`

```cpp
struct TextSize {
    int width = 0;
    int height = 0;
};
```

渲染后的像素尺寸，由 `measureText()` 返回。空串/非法字库时返回 `{0, 0}`。

### 3.5 `AutoRegister`

```cpp
struct AutoRegister {
    explicit AutoRegister(const FontDesc& font) noexcept;
};
```

构造时调用 `dbtext::registerFont(font)`。字库 header 末尾会定义一个 `inline const AutoRegister kAutoRegister{ kFont };`，因此**包含 header 即完成注册**。

> 依赖静态初始化。若把字库 header 包在静态库里又只引用了部分符号，链接器可能丢弃该目标文件而漏掉注册；这种情况下请显式调用 `dbtext::registerFont(dbtext::fonts::xxx::kFont)`。

### 3.6 `FontScope`

```cpp
struct FontScope {
    explicit FontScope(FontID id) noexcept;              // 切换失败则保持原样
    explicit FontScope(const FontDesc& font) noexcept;   // font.valid() 时才切换
    ~FontScope() noexcept;                               // 恢复进入作用域前的字库
};
```

RAII 临时切换当前线程的默认字库，构造时保存旧值、析构时恢复。不可拷贝。

```cpp
{
    dbtext::FontScope scope("fusion-pixel-8px-mono-zh_hans");
    paintDBText(renderer, 10, 10, {0, 0, 0, 255}, 2.0f, "小字");
}   // 离开作用域后自动恢复原来的字库
```

---

## 4. 常量

| 名称 | 类型 | 值 | 说明 |
|---|---|---|---|
| `dbtext::kFontFormatVersion` | `uint16_t` | `2` | 当前字库格式版本 |
| `dbtext::kNoGlyphIndex` | `uint32_t` | `0xFFFFFFFF` | `glyphIndex()` 无结果时的返回值 |
| `dbtext::kTransparent` | `SDL_Color` | `{0, 0, 0, 0}` | 全透明颜色；作为背景色表示「不绘制背景」 |
| `dbtext::ttf::PIXELS_TO_UNITS` | `int` | `20` | 导出 TTF 时，1 个点阵像素对应的字体单位数 |
| `dbtext::ttf::MAX_GLYPHS` | `uint32_t` | `65535` | 导出 TTF 的字形数量上限 |

---

## 5. 字库注册与查询

| 函数 | 说明 |
|---|---|
| `bool registerFont(const FontDesc& font) noexcept` | 注册字库。`font` 必须有效且 ID 未被占用；重复注册同一对象/同一 ID 返回 `false` 且不产生副作用 |
| `const FontDesc* findFont(FontID id) noexcept` | 按 ID 查找，未注册返回 `nullptr` |
| `std::vector<FontID> fontIds()` | 列出所有已注册字库的 ID（按注册顺序） |
| `bool setFont(FontID id) noexcept` | 设置当前**线程**的默认字库；ID 不存在返回 `false`，当前字库不变 |
| `const FontDesc* currentFont() noexcept` | 获取当前字库。查找顺序：`setFont()` 设置 → `DBTEXT_DEFAULT_FONT_ID` → 第一个注册的字库；一个都没有则返回 `nullptr` |
| `FontID currentFontId() noexcept` | 当前字库的 ID；无字库时返回空 `string_view` |

> 「当前字库」是 `thread_local` 的，不同线程可以各用各的；注册表本身不做加锁，应在启动阶段（或多线程开始前）完成注册。

示例：遍历所有已注册字库

```cpp
for (dbtext::FontID id : dbtext::fontIds()) {
    const dbtext::FontDesc* font = dbtext::findFont(id);
    printf("%s: %u 个字形, 单元格 %ux%u\n",
           font->id, font->glyphCount, font->cellWidth, font->cellHeight);
}
```

---

## 6. 字形查询

| 函数 | 说明 |
|---|---|
| `const GlyphInfo* resolveGlyph(const FontDesc& font, uint32_t codepoint, GlyphInfo& scratch) noexcept` | 带**回退链**的字形解析：先查 `codepoint`，再查 `DBTEXT_FALLBACK_CODEPOINT`（默认 U+FFFD）、`'?'`、`' '`；全部失败返回 `nullptr`。返回的指针指向 `scratch`，仅在本次调用期间有效 |

底层可用 `FontDesc::findGlyph()`（见 [3.3](#33-fontdesc)）做**不带回退**的精确查询，例如实现自己的缺字统计：

```cpp
dbtext::GlyphInfo glyph;
if (!font->findGlyph(0x4E2D, glyph)) {
    // 该字库没有「中」
}
```

---

## 7. 度量与渲染

### 7.1 `measureText`

```cpp
dbtext::TextSize measureText(const FontDesc& font, const char* text) noexcept;
```

计算文本渲染后的像素尺寸。

- 支持 `\n` 换行、`\t` 制表（步进 `FontDesc::tabWidth`）、忽略 `\r`。
- 行高 = 该行内字形高度的最大值，至少为 `FontDesc::cellHeight`。
- `text` 为 `nullptr`、空串或 `font` 无效时返回 `{0, 0}`。

### 7.2 `createTextSurface`

```cpp
SDL_Surface* createTextSurface(const FontDesc& font, const char* text,
                               const SDL_Color& fgColor,
                               const SDL_Color& bgColor = kTransparent);

SDL_Surface* createTextSurface(const FontDesc* font, const char* text,
                               const SDL_Color& fgColor,
                               const SDL_Color& bgColor = kTransparent);   // 指针重载
```

| 参数 | 说明 |
|---|---|
| `font` | 字库（指针重载传 `nullptr` 时返回 `nullptr`） |
| `text` | UTF-8 文本，支持 `\n` / `\t` |
| `fgColor` | **前景色**：墨点写入的颜色（含 alpha） |
| `bgColor` | **背景色**：先铺满整个表面再画墨点；默认 `kTransparent`（alpha = 0）表示背景透明 |

- 返回 **RGBA8888** 表面，尺寸来自 `measureText()`。
- 背景色覆盖整个文本包围盒（含行间空隙），多行文本短行右侧也会填背景。
- 多行文本按行左对齐，`\n` 后回到 `x = 0`。
- 失败（空串、`font` 为空、`SDL_CreateSurface` 失败）返回 `nullptr`。
- 调用方负责 `SDL_DestroySurface()`。

### 7.3 `paintText`

每个重载都有「双色」与「单色」两个版本，单色版本等价于 `bgColor = kTransparent`。

```cpp
// 双色
SDL_Rect paintText(SDL_Renderer* renderer, const FontDesc& font,
                   int ux, int uy, const SDL_Color& fgColor, const SDL_Color& bgColor,
                   float ptsize, const char* text, const SDL_FPoint& center = {0.0f, 0.0f});
SDL_Rect paintText(SDL_Renderer* renderer, FontID fontId,
                   int ux, int uy, const SDL_Color& fgColor, const SDL_Color& bgColor,
                   float ptsize, const char* text, const SDL_FPoint& center = {0.0f, 0.0f});

// 单色（背景透明）
SDL_Rect paintText(SDL_Renderer* renderer, const FontDesc& font,
                   int ux, int uy, const SDL_Color& fgColor, float ptsize,
                   const char* text, const SDL_FPoint& center = {0.0f, 0.0f});
SDL_Rect paintText(SDL_Renderer* renderer, FontID fontId,
                   int ux, int uy, const SDL_Color& fgColor, float ptsize,
                   const char* text, const SDL_FPoint& center = {0.0f, 0.0f});
```

| 参数 | 说明 |
|---|---|
| `renderer` | 目标 SDL 渲染器 |
| `font` / `fontId` | 字库对象，或字库 ID（内部 `findFont()`，找不到直接返回失败） |
| `ux`, `uy` | 文本位置，**未扣除** `center` 偏移 |
| `fgColor` | 前景色（墨点颜色，含 alpha） |
| `bgColor` | 背景色（含 alpha）；`kTransparent` = 不画背景 |
| `ptsize` | 缩放**倍数**，`1.0f` = 原始像素大小 |
| `text` | UTF-8 文本，支持 `\n` / `\t` |
| `center` | 锚点比例：`{0,0}` 左上、`{0.5,0.5}` 居中、`{1,1}` 右下 |

- 内部流程：`createTextSurface` → `SDL_CreateTextureFromSurface` → `SDL_SetTextureScaleMode(NEAREST)` → `SDL_RenderTexture` → 立即释放 surface/texture。
- 返回实际绘制区域（像素，已应用 `center` 偏移）；失败返回 `{-1, -1, -1, -1}`。
- 使用 `NEAREST` 缩放，非整数倍时会出现像素不齐，建议用整数倍。

### 7.4 `paintTextFormat`

同样有「双色」/「单色」两种重载，`FontDesc` 与 `FontID` 各一套。

```cpp
// 双色
SDL_Rect paintTextFormat(SDL_Renderer* renderer, const FontDesc& font,
                         int ux, int uy, SDL_Color fgColor, SDL_Color bgColor,
                         float ptsize, const SDL_FPoint& center, const char* format, ...);
SDL_Rect paintTextFormat(SDL_Renderer* renderer, FontID fontId,
                         int ux, int uy, SDL_Color fgColor, SDL_Color bgColor,
                         float ptsize, const SDL_FPoint& center, const char* format, ...);

// 单色（背景透明）
SDL_Rect paintTextFormat(SDL_Renderer* renderer, const FontDesc& font,
                         int ux, int uy, SDL_Color fgColor, float ptsize,
                         const SDL_FPoint& center, const char* format, ...);
SDL_Rect paintTextFormat(SDL_Renderer* renderer, FontID fontId,
                         int ux, int uy, SDL_Color fgColor, float ptsize,
                         const SDL_FPoint& center, const char* format, ...);
```

`printf` 风格的 `paintText`。

- 格式化结果最长 **1024 字节**，超长会被截断；格式化失败时按空串处理。
- 注意 `center` 是**必填的位置参数**（因为后面是可变参数），不能省略。
- 返回值与失败约定同 `paintText`。

---

## 8. 旧版兼容接口

定义 `DBTEXT_NO_LEGACY_API` 可关闭。它们等价于「使用 `dbtext::currentFont()` 的新接口」。

| 旧接口 | 等价的新接口 |
|---|---|
| `SDL_Surface* createDBTextSurface(const char* text, const SDL_Color& color)` | `dbtext::createTextSurface(dbtext::currentFont(), text, color)` |
| `SDL_Rect paintDBText(SDL_Renderer*, int ux, int uy, const SDL_Color&, float ptsize, const char* text, const SDL_FPoint& center = {0,0})` | `dbtext::paintText(renderer, *dbtext::currentFont(), ...)` |
| `SDL_Rect paintDBTextFormat(SDL_Renderer*, int ux, int uy, SDL_Color, float ptsize, const SDL_FPoint& center, const char* text, ...)` | `dbtext::paintTextFormat(renderer, *dbtext::currentFont(), ...)` |

与 v1 的差异：

- 旧接口**只接受一个颜色（前景色）**，背景始终透明；需要背景色请使用新的 `dbtext::paintText*`。
- 行为等价（同样的换行/Tab/缩放语义），但实现改为流式 UTF-8 解码（不再每次分配 `std::vector`）、只写墨点像素。
- `paintDBTextFormat` 的缓冲区由 256 字节提升到 1024 字节。
- 缺字处理由「画第 1 号字形」改为回退链（U+FFFD → `'?'` → `' '`）。

---

## 9. TTF 导出（可选）

在包含 `DebugText.hpp` 前定义 `DBTEXT_ENABLE_TTF_GENERATION`，并链接 SDL3_ttf。

用途：把内嵌点阵字库**现场转换成内存 TTF**，以便交给 SDL_ttf / 其它 TTF 消费方（例如富文本排版、旋转缩放）。

### 9.1 类型

```cpp
struct Geometry {
    int cellHeight = 0;   //!< 单元格高（像素）
    int unitsPerEm = 0;   //!< = cellHeight * PIXELS_TO_UNITS
    int ascender = 0;     //!< = unitsPerEm
    int descender = 0;    //!< = 0
};

Geometry makeGeometry(const FontDesc& font);
```

### 9.2 函数

| 函数 | 说明 |
|---|---|
| `std::vector<uint8_t> buildTTF(const FontDesc& font)` | 生成完整的 sfnt 字节流；失败返回空 vector。字形数 = `maxCodepoint + 1`（上限 `MAX_GLYPHS`），字形 ID 与码点相同 |
| `const std::vector<uint8_t>& getFontData(const FontDesc& font)` | 带缓存的 `buildTTF()`，每个字库只生成一次；返回的引用在程序生命周期内稳定 |
| `TTF_Font* openFont(const FontDesc& font, float ptsize = 0.0f)` | 用 `SDL_IOFromMem` + `TTF_OpenFontIO()` 打开生成的 TTF，并设置 `TTF_HINTING_NONE`。`ptsize = 0` 表示与点阵 1:1（= `font.cellHeight`）。失败返回 `nullptr` |
| `bool saveToFile(const FontDesc& font, const char* path)` | 把生成的 TTF 写到磁盘（调试用） |
| `TTF_Font* openDebugFont(float ptsize = 0.0f)` | 旧接口：对 `dbtext::currentFont()` 调用 `openFont()` |

- 生成的字体名固定为 `DebugBitmapFont`（派生字体不沿用 Fusion Pixel 的保留字体名，符合 OFL 要求）。
- 返回的 `TTF_Font*` 需要调用方 `TTF_CloseFont()`；字体使用的内存由 `getFontData()` 缓存持有，生命周期安全。
- 使用前需已 `TTF_Init()`。

示例：

```cpp
#define DBTEXT_ENABLE_TTF_GENERATION
#include "DebugText.hpp"
#include "fonts/fusion_pixel_12px_mono_zh_hans.hpp"

TTF_Font* font = dbtext::ttf::openFont(dbtext::fonts::fusion_pixel_12px_mono_zh_hans::kFont, 24.0f);
if (font != nullptr) {
    SDL_Surface* s = TTF_RenderText_Solid(font, "Hello 中文", 0, SDL_Color{255, 255, 255, 255});
    // ... 使用 s ...
    SDL_DestroySurface(s);
    TTF_CloseFont(font);
}
```

---

## 10. 解码辅助函数

```cpp
inline std::vector<uint32_t> utf8_to_codepoints_noexcept(const std::string& utf8_str) noexcept;
```

把 UTF-8 字符串解码为码点数组；遇到非法序列时返回**已成功解码的前缀**。

- 该函数位于全局命名空间，仅为兼容旧代码保留（`#ifndef __INC_DECODE_` 保护，可与 `decode.hpp` 共存）。
- 渲染路径内部使用流式解码（`dbtext::detail::decodeUtf8`），不再经由此函数。

---

## 11. 字库 header 结构与自写字库

一个生成的字库 header 形如：

```cpp
#pragma once
#include <cstdint>
#include "DebugText.hpp"

namespace dbtext::fonts::fusion_pixel_12px_mono_zh_hans {

inline constexpr std::uint32_t kPresenceBits[]   = { /* ... */ };
inline constexpr std::uint32_t kPresenceBlocks[] = { /* ... */ };
inline constexpr std::uint32_t kFullWidthBits[]  = { /* ... */ };
inline constexpr std::uint32_t kFullWidthBlocks[]= { /* ... */ };
inline constexpr std::uint32_t kAtlas[]          = { /* ... */ };
/* extras 相关数组 ... */

inline constexpr FontDesc makeFont() noexcept { FontDesc font{}; /* 逐字段赋值 */ return font; }
inline constexpr FontDesc kFont = makeFont();
inline const AutoRegister kAutoRegister{ kFont };

} // namespace dbtext::fonts::fusion_pixel_12px_mono_zh_hans
```

- 命名空间名由 FontID 转换而来（非字母数字换成 `_`）。
- `kFont` 必须有静态存储期（注册表保存的是它的地址）。
- 想加自己的字库：优先用 generator（见 README）；只有特殊需求（例如运行期生成、只有几十个字形）才建议手写 `FontDesc`，此时仍需保持同样的位图打包约定：**按行紧密排列、1 = 墨点、半角/全角记录长度分别为 `halfRecordBytes` / `fullRecordBytes`**。

生成字库的命令：

```bash
# 内置预设（8/10/12px mono zh_hans）
cmake --build --preset debug --target fonts

# 任意 TTF
bin/debug/generator.exe --ttf path/to/font.ttf --id my-font \
    --out include/fonts/my_font.hpp --ptsize 16 --preview
```

---

## 12. 行为细节与注意事项

> `dbtext::detail::*`（`popcount32` / `popcountBefore` / `popcountInclusive` / `decodeUtf8` / `fontRegistry` / `currentFontSlot`）属于实现细节，不保证跨版本兼容，请勿直接依赖。

**文本语义**

- 支持 UTF-8，含辅助平面（如 CJK 扩展 B）；非法字节序列按 U+FFFD 处理。
- `\n` 换行（行高取本行最大字形高度，至少 `cellHeight`）；`\t` 前进 `tabWidth`；`\r` 被忽略；其它控制字符若无字形则走回退链。
- 对齐方式：每个字形从当前行顶部开始绘制，最左对齐，按 `width`（= advance）步进。
- 文本以 `\n` 结尾时会多出一个空行的高度（与 v1 行为一致）。

**返回值约定**

| 接口 | 失败表现 |
|---|---|
| `findFont` / `currentFont` | `nullptr` |
| `createTextSurface` | `nullptr` |
| `paintText` / `paintTextFormat` / 旧接口 | `{-1, -1, -1, -1}` |
| `setFont` / `registerFont` / `saveToFile` | `false` |
| `dbtext::ttf::openFont` | `nullptr`（可用 `SDL_GetError()` 查看原因） |

**性能**

- 字形查询是纯位运算（两次 `popcount` + 一次乘加），无哈希、无分配。
- `paintText*` 每次调用都会重建 surface 与 texture；HUD 这类高频固定文本可以自己缓存 `createTextSurface()` 的结果或 texture。
- 缩放使用 `NEAREST`，整数倍最清晰。

**线程**

- 注册表不做同步，应在多线程开始前完成注册（字库 header 的静态注册天然满足）。
- 「当前字库」是 `thread_local`，`setFont()` / `FontScope` 只影响当前线程。
- 渲染接口本身是无状态的，可在不同线程操作**各自的** `SDL_Renderer`。

**许可**

- 内置字库派生自 Fusion Pixel（SIL OFL 1.1），`FontDesc` 里带有 `copyright` / `licenseName` 字段，分发时请一并保留。
- 用 `dbtext::ttf` 再导出 TTF 时不要沿用保留字体名（内置导出名为 `DebugBitmapFont`）。
