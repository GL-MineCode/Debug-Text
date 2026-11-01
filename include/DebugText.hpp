#ifndef __INC_DEBUGTEXT_
#define __INC_DEBUGTEXT_

/**
 * @file DebugText.hpp
 * @author Gao Li (https://github.com/GL-MineCode)
 * @brief A minimalist and lightweight text (bitmap font) drawing library based on SDL3, supporting the UTF-8 character set
 * @version 2.0
 * @date 2026-11-01
 *
 * @copyright Copyright (c) 2026
 *
 * @details Licensed under the MIT License. See LICENSE file for full license.
 *
 * @note v2.0 变化：
 *       - 字库不再写死在本头文件里，而是由 generator 直接生成的「字库 header」提供，
 *         每个字库 header 通过 dbtext::AutoRegister 自动注册，可同时引用多个；
 *       - 渲染接口支持按 FontID（字符串）选择字库；
 *       - 字库数据格式升级为 v2（紧凑位图索引，见下方格式说明）。
 *
 * @par 字库数据格式 v2（全部为常量数组，直接放在 .rodata）
 *      - presenceBits/presenceBlockCounts：码点 → 字形序号（位图 + 每 256 码点累计计数，popcount 得到序号）
 *      - fullWidthBits/fullWidthBlockCounts：字形序号 → 半角/全角（1 bit/字形）
 *      - atlas：字形位图按码点升序紧密排列，记录长度 = ceil(w*h/8)（半角 recordBytes / 全角 recordBytes）
 *      - extra*：少数「高度/宽度超格」字形单独存放
 *      位意义：1 = 墨点（v1 的 0 = 墨点已被废弃）。
 */

#include <SDL3/SDL.h>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief 默认字库 ID（在没有调用过 dbtext::setFont 时使用）
 * @note 可在包含本头文件前用 -D 覆盖
 */
#ifndef DBTEXT_DEFAULT_FONT_ID
#define DBTEXT_DEFAULT_FONT_ID "fusion-pixel-12px-mono-zh_hans"
#endif

/**
 * @brief 缺字时优先回退的码点（U+FFFD REPLACEMENT CHARACTER）
 */
#ifndef DBTEXT_FALLBACK_CODEPOINT
#define DBTEXT_FALLBACK_CODEPOINT 0xFFFDu
#endif

#ifndef __INC_DECODE_

/**
 * @brief Convert a UTF-8 encoded string to a vector of Unicode codepoints
 *
 * Decodes a UTF-8 string into its individual Unicode codepoints.
 * Performs validation at each step and returns whatever has been
 * successfully decoded so far if an invalid sequence is encountered.
 *
 * @param utf8_str The UTF-8 encoded input string
 * @return std::vector<uint32_t> A vector of Unicode codepoints decoded from the input
 *
 * @note 渲染路径已改为流式解码（见 dbtext::detail::decodeUtf8），此函数仅为兼容旧代码保留。
 */
inline std::vector<uint32_t> utf8_to_codepoints_noexcept(const std::string& utf8_str) noexcept {
    std::vector<uint32_t> codepoints;

    for (size_t i = 0; i < utf8_str.size(); ) {
        uint8_t first_byte = static_cast<uint8_t>(utf8_str[i]);

        size_t char_len = 0;
        uint32_t codepoint = 0;

        if (first_byte <= 0x7F) {
            char_len = 1;
            codepoint = first_byte;
        } else if ((first_byte & 0xE0) == 0xC0) {
            char_len = 2;
            codepoint = first_byte & 0x1F;
        } else if ((first_byte & 0xF0) == 0xE0) {
            char_len = 3;
            codepoint = first_byte & 0x0F;
        } else if ((first_byte & 0xF8) == 0xF0) {
            char_len = 4;
            codepoint = first_byte & 0x07;
        } else {
            return codepoints;
        }

        if (i + char_len > utf8_str.size()) {
            return codepoints;
        }

        for (size_t j = 1; j < char_len; ++j) {
            uint8_t next_byte = static_cast<uint8_t>(utf8_str[i + j]);

            if ((next_byte & 0xC0) != 0x80) {
                return codepoints;
            }

            codepoint = (codepoint << 6) | (next_byte & 0x3F);
        }

        if ((char_len == 1 && codepoint > 0x7F) ||
            (char_len == 2 && (codepoint < 0x80 || codepoint > 0x7FF)) ||
            (char_len == 3 && (codepoint < 0x800 || codepoint > 0xFFFF)) ||
            (char_len == 4 && (codepoint < 0x10000 || codepoint > 0x10FFFF))) {
            return codepoints;
        }

        codepoints.push_back(codepoint);
        i += char_len;
    }

    return codepoints;
}

#endif // __INC_DECODE_

namespace dbtext {

using FontID = std::string_view;

//! 字库格式版本
inline constexpr uint16_t kFontFormatVersion = 2;
//! glyphIndex() 的无效返回值
inline constexpr uint32_t kNoGlyphIndex = 0xFFFFFFFFu;

namespace detail {

// ---------------------------- 基础工具 ----------------------------

inline uint32_t popcount32(uint32_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<uint32_t>(__builtin_popcount(value));
#else
    value = value - ((value >> 1) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2) & 0x33333333u);
    value = (value + (value >> 4)) & 0x0F0F0F0Fu;
    return (value * 0x01010101u) >> 24;
#endif
}

/**
 * @brief 统计位图中 [0, index) 范围内 1 的个数
 * @param bits        位图（每 256 项为一个块，共 8 个 32 位词，且已补齐到整块）
 * @param blockCounts blockCounts[b] = 第 b 块之前的 1 的总数
 */
inline uint32_t popcountBefore(const uint32_t* bits, const uint32_t* blockCounts, uint32_t index) noexcept {
    const uint32_t block = index >> 8;
    const uint32_t* base = bits + (block << 3);
    const uint32_t wordInBlock = (index & 255u) >> 5;
    uint32_t total = blockCounts[block];
    for (uint32_t i = 0; i < wordInBlock; ++i) {
        total += popcount32(base[i]);
    }
    const uint32_t bit = index & 31u;
    if (bit != 0u) {
        // bit ∈ [1, 31]，1u << bit 不会溢出；bit == 31 时掩码为 0x7FFFFFFF（不能含 bit 31 本身）
        const uint32_t mask = (1u << bit) - 1u;
        total += popcount32(base[wordInBlock] & mask);
    }
    return total;
}

/**
 * @brief 统计位图中 [0, index] 范围内 1 的个数（包含 index 自身）
 */
inline uint32_t popcountInclusive(const uint32_t* bits, const uint32_t* blockCounts, uint32_t index) noexcept {
    const uint32_t block = index >> 8;
    const uint32_t* base = bits + (block << 3);
    const uint32_t wordInBlock = (index & 255u) >> 5;
    uint32_t total = blockCounts[block];
    for (uint32_t i = 0; i < wordInBlock; ++i) {
        total += popcount32(base[i]);
    }
    const uint32_t bit = index & 31u;
    const uint32_t mask = (bit == 31u) ? 0xFFFFFFFFu : ((2u << bit) - 1u);
    total += popcount32(base[wordInBlock] & mask);
    return total;
}

/**
 * @brief 流式解码一个 UTF-8 码点
 *
 * @param cursor 输入/输出：当前读取位置，成功时前进 char_len 字节
 * @param end    字符串结束位置
 * @param codepoint 输出：解码出的码点（非法序列输出 U+FFFD）
 * @return int 消耗的字节数（0 表示已到末尾）
 */
inline int decodeUtf8(const char*& cursor, const char* end, uint32_t& codepoint) noexcept {
    if (cursor >= end) return 0;

    const uint8_t first = static_cast<uint8_t>(*cursor);
    if (first <= 0x7Fu) {
        codepoint = first;
        ++cursor;
        return 1;
    }

    size_t length = 0;
    uint32_t value = 0;
    if ((first & 0xE0u) == 0xC0u) {
        length = 2; value = first & 0x1Fu;
    } else if ((first & 0xF0u) == 0xE0u) {
        length = 3; value = first & 0x0Fu;
    } else if ((first & 0xF8u) == 0xF0u) {
        length = 4; value = first & 0x07u;
    } else {
        codepoint = 0xFFFDu;
        ++cursor;
        return 1;
    }

    if (static_cast<size_t>(end - cursor) < length) {
        codepoint = 0xFFFDu;
        ++cursor;
        return 1;
    }

    for (size_t i = 1; i < length; ++i) {
        const uint8_t next = static_cast<uint8_t>(cursor[i]);
        if ((next & 0xC0u) != 0x80u) {
            codepoint = 0xFFFDu;
            ++cursor;
            return 1;
        }
        value = (value << 6) | (next & 0x3Fu);
    }

    static constexpr uint32_t kMinCode[5] = { 0u, 0u, 0x80u, 0x800u, 0x10000u };
    if (value < kMinCode[length] || value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu)) {
        codepoint = 0xFFFDu;
        ++cursor;
        return 1;
    }

    codepoint = value;
    cursor += length;
    return static_cast<int>(length);
}

} // namespace detail

// ---------------------------- 字形与字库描述 ----------------------------

/**
 * @brief 一个字形的位图视图
 *
 * @note words/byteOffset 描述的是「打包字节流」：第 i 个字节 = (words[(byteOffset+i)>>2] >> (8*((byteOffset+i)&3))) & 0xFF，
 *       这样既与主机字节序无关，也能放进 constexpr 初始化。
 */
struct GlyphInfo {
    uint16_t width = 0;       //!< 位图宽度（像素），同时也是前进量 advance
    uint16_t height = 0;      //!< 位图高度（像素）
    const uint32_t* words = nullptr;  //!< 打包字节流的 32 位词视图
    uint32_t byteOffset = 0;  //!< 本字形位图在字节流中的起始字节偏移

    inline uint8_t byteAt(uint32_t index) const noexcept {
        const uint32_t pos = byteOffset + index;
        return static_cast<uint8_t>((words[pos >> 2] >> (8u * (pos & 3u))) & 0xFFu);
    }

    //! 按行紧密排列（每行 width 位连续）读取第 index 位，1 = 墨点
    inline bool bitAt(uint32_t index) const noexcept {
        const uint32_t pos = byteOffset + (index >> 3);
        const uint32_t shift = 8u * (pos & 3u) + (7u - (index & 7u));
        return ((words[pos >> 2] >> shift) & 1u) != 0u;
    }
};

/**
 * @brief 字库描述（由 generator 生成的「字库 header」提供）
 */
struct FontDesc {
    // ---------------- 头部信息 ----------------
    uint16_t formatVersion = 0;
    uint16_t flags = 0;                   //!< bit0 = 1 表示位图中 1 为墨点
    const char* id = "";                  //!< FontID（唯一）
    const char* displayName = "";         //!< 人类可读名称
    const char* styleName = "";           //!< 字重/风格，如 Regular
    const char* copyright = "";           //!< 版权声明
    const char* licenseName = "";         //!< 许可名称，如 SIL Open Font License 1.1
    const char* sourceFile = "";          //!< 源字体文件名
    const char* sourceVersion = "";       //!< 源字体版本

    // ---------------- 度量 ----------------
    uint8_t cellWidth = 0;                //!< 全角字宽（像素）
    uint8_t cellHeight = 0;               //!< 位图高 / 默认行高（像素）
    uint8_t halfWidth = 0;                //!< 半角字宽（像素）
    uint8_t tabWidth = 0;                 //!< Tab 步进（像素）

    // ---------------- 码点 → 字形序号 ----------------
    uint32_t maxCodepoint = 0;
    uint32_t glyphCount = 0;
    uint32_t presenceWordCount = 0;
    const uint32_t* presenceBits = nullptr;
    const uint32_t* presenceBlockCounts = nullptr;

    // ---------------- 字形序号 → 半角/全角 ----------------
    uint32_t fullWidthWordCount = 0;
    const uint32_t* fullWidthBits = nullptr;
    const uint32_t* fullWidthBlockCounts = nullptr;

    // ---------------- 位图负载 ----------------
    uint8_t halfRecordBytes = 0;          //!< 半角字形记录字节数 = ceil(halfWidth*cellHeight/8)
    uint8_t fullRecordBytes = 0;          //!< 全角字形记录字节数 = ceil(cellWidth*cellHeight/8)
    const uint32_t* atlas = nullptr;      //!< 位图字节流（按码点升序排列）

    // ---------------- 超格字形（可选） ----------------
    uint32_t extraCount = 0;
    const uint32_t* extraBits = nullptr;
    const uint32_t* extraBlockCounts = nullptr;
    const uint16_t* extraDims = nullptr;     //!< w | (h << 8)
    const uint32_t* extraOffsets = nullptr;  //!< 相对 extraData 的字节偏移
    const uint32_t* extraData = nullptr;

    // ---------------- 查询 ----------------

    inline bool valid() const noexcept {
        return formatVersion == kFontFormatVersion
            && id != nullptr && id[0] != '\0'
            && presenceBits != nullptr && presenceBlockCounts != nullptr
            && fullWidthBits != nullptr && fullWidthBlockCounts != nullptr
            && atlas != nullptr
            && cellWidth != 0 && cellHeight != 0
            && halfRecordBytes != 0 && fullRecordBytes != 0;
    }

    /**
     * @brief 码点 → 字形序号（无对应字形时返回 kNoGlyphIndex）
     */
    inline uint32_t glyphIndex(uint32_t codepoint) const noexcept {
        if (presenceBits == nullptr || codepoint > maxCodepoint) return kNoGlyphIndex;
        if (((presenceBits[codepoint >> 5] >> (codepoint & 31u)) & 1u) == 0u) return kNoGlyphIndex;
        return detail::popcountInclusive(presenceBits, presenceBlockCounts, codepoint) - 1u;
    }

    //! 字形序号 index 之前的全角字形数量
    inline uint32_t fullWidthBefore(uint32_t index) const noexcept {
        return detail::popcountBefore(fullWidthBits, fullWidthBlockCounts, index);
    }

    inline bool isFullWidth(uint32_t index) const noexcept {
        return ((fullWidthBits[index >> 5] >> (index & 31u)) & 1u) != 0u;
    }

    inline bool isExtra(uint32_t index) const noexcept {
        return extraCount != 0u && extraBits != nullptr
            && ((extraBits[index >> 5] >> (index & 31u)) & 1u) != 0u;
    }

    /**
     * @brief 查询码点对应的字形
     * @return 找到返回 true 并填充 out
     */
    inline bool findGlyph(uint32_t codepoint, GlyphInfo& out) const noexcept {
        const uint32_t index = glyphIndex(codepoint);
        if (index == kNoGlyphIndex) return false;

        if (isExtra(index)) {
            const uint32_t slot = detail::popcountBefore(extraBits, extraBlockCounts, index);
            if (slot >= extraCount) return false;
            const uint32_t dims = extraDims[slot];
            out.width = static_cast<uint16_t>(dims & 0xFFu);
            out.height = static_cast<uint16_t>(dims >> 8);
            out.words = extraData;
            out.byteOffset = extraOffsets[slot];
            return true;
        }

        const uint32_t fullBefore = fullWidthBefore(index);
        uint64_t offset = static_cast<uint64_t>(fullRecordBytes) * fullBefore
                        + static_cast<uint64_t>(halfRecordBytes) * (index - fullBefore);
        const bool full = isFullWidth(index);
        out.width = full ? cellWidth : halfWidth;
        out.height = cellHeight;
        out.words = atlas;
        out.byteOffset = static_cast<uint32_t>(offset);
        return true;
    }

    inline bool findGlyph(uint32_t codepoint, GlyphInfo& out, uint32_t fallbackCodepoint) const noexcept {
        if (findGlyph(codepoint, out)) return true;
        if (fallbackCodepoint != codepoint && findGlyph(fallbackCodepoint, out)) return true;
        return false;
    }
};

// ---------------------------- 字库注册表 ----------------------------

namespace detail {

inline std::vector<const FontDesc*>& fontRegistry() noexcept {
    static std::vector<const FontDesc*> fonts;
    return fonts;
}

inline const FontDesc*& currentFontSlot() noexcept {
    static thread_local const FontDesc* current = nullptr;
    return current;
}

} // namespace detail

/**
 * @brief 注册字库（重复注册同一个 FontID 会被忽略）
 * @return 本次是否真的注册成功
 */
inline bool registerFont(const FontDesc& font) noexcept {
    if (!font.valid()) return false;
    auto& registry = detail::fontRegistry();
    for (const FontDesc* existing : registry) {
        if (existing == &font) return false;
        if (std::strcmp(existing->id, font.id) == 0) return false;
    }
    registry.push_back(&font);
    return true;
}

/**
 * @brief 按 FontID 查找字库（未注册返回 nullptr）
 */
inline const FontDesc* findFont(FontID id) noexcept {
    for (const FontDesc* font : detail::fontRegistry()) {
        if (id.compare(font->id) == 0) return font;
    }
    return nullptr;
}

//! 列出所有已注册字库的 FontID
inline std::vector<FontID> fontIds() {
    std::vector<FontID> ids;
    for (const FontDesc* font : detail::fontRegistry()) ids.emplace_back(font->id);
    return ids;
}

/**
 * @brief 设置当前线程使用的默认字库
 * @return FontID 不存在时返回 false（当前字库不变）
 */
inline bool setFont(FontID id) noexcept {
    const FontDesc* font = findFont(id);
    if (font == nullptr) return false;
    detail::currentFontSlot() = font;
    return true;
}

/**
 * @brief 获取当前线程使用的字库
 *
 * 查找顺序：setFont() 设置 → DBTEXT_DEFAULT_FONT_ID → 第一个注册的字库
 */
inline const FontDesc* currentFont() noexcept {
    const FontDesc*& slot = detail::currentFontSlot();
    if (slot != nullptr && slot->valid()) return slot;
    slot = findFont(FontID(DBTEXT_DEFAULT_FONT_ID));
    if (slot == nullptr) {
        const auto& registry = detail::fontRegistry();
        if (!registry.empty()) slot = registry.front();
    }
    return slot;
}

//! 当前字库的 FontID（无字库时返回空串）
inline FontID currentFontId() noexcept {
    const FontDesc* font = currentFont();
    return font ? FontID(font->id) : FontID();
}

/**
 * @brief 让字库 header 里的静态对象在包含时自动完成注册
 */
struct AutoRegister {
    explicit AutoRegister(const FontDesc& font) noexcept { registerFont(font); }
};

/**
 * @brief RAII 临时切换当前字库
 *
 * @code
 * {
 *     dbtext::FontScope scope("fusion-pixel-8px-mono-zh_hans");
 *     paintDBTextFormat(renderer, 10, 10, color, 2, {0, 0}, "FPS: %.0f", fps);
 * }
 * @endcode
 */
struct FontScope {
    const FontDesc* previous;

    explicit FontScope(FontID id) noexcept : previous(detail::currentFontSlot()) {
        setFont(id); // 失败时保持原状
    }

    explicit FontScope(const FontDesc& font) noexcept : previous(detail::currentFontSlot()) {
        if (font.valid()) detail::currentFontSlot() = &font;
    }

    ~FontScope() noexcept { detail::currentFontSlot() = previous; }

    FontScope(const FontScope&) = delete;
    FontScope& operator=(const FontScope&) = delete;
};

// ---------------------------- 度量与渲染 ----------------------------

struct TextSize {
    int width = 0;
    int height = 0;
};

//! 全透明颜色（作为背景色时表示「不绘制背景」）
inline constexpr SDL_Color kTransparent = { 0, 0, 0, 0 };

/**
 * @brief 解析码点对应的字形（带回退链：自身 → U+FFFD → '?' → ' '）
 */
inline const GlyphInfo* resolveGlyph(const FontDesc& font, uint32_t codepoint, GlyphInfo& scratch) noexcept {
    if (font.findGlyph(codepoint, scratch)) return &scratch;
    if (font.findGlyph(static_cast<uint32_t>(DBTEXT_FALLBACK_CODEPOINT), scratch)) return &scratch;
    if (font.findGlyph(static_cast<uint32_t>('?'), scratch)) return &scratch;
    if (font.findGlyph(static_cast<uint32_t>(' '), scratch)) return &scratch;
    return nullptr;
}

/**
 * @brief 计算文本渲染后的像素尺寸（支持 \n 换行与 \t 制表）
 */
inline TextSize measureText(const FontDesc& font, const char* text) noexcept {
    TextSize size;
    if (text == nullptr || !font.valid()) return size;

    const char* cursor = text;
    const char* end = text + std::strlen(text);

    int lineWidth = 0;
    int lineHeight = font.cellHeight;
    int maxWidth = 0;
    int totalHeight = 0;
    GlyphInfo scratch;

    while (cursor < end) {
        uint32_t codepoint = 0;
        if (detail::decodeUtf8(cursor, end, codepoint) <= 0) break;

        if (codepoint == '\n') {
            if (lineWidth > maxWidth) maxWidth = lineWidth;
            totalHeight += lineHeight;
            lineWidth = 0;
            lineHeight = font.cellHeight;
            continue;
        }
        if (codepoint == '\t') {
            lineWidth += font.tabWidth != 0 ? font.tabWidth : font.cellWidth;
            continue;
        }
        if (codepoint == '\r') continue;

        const GlyphInfo* glyph = resolveGlyph(font, codepoint, scratch);
        if (glyph == nullptr) continue;

        lineWidth += glyph->width;
        if (static_cast<int>(glyph->height) > lineHeight) lineHeight = static_cast<int>(glyph->height);
    }

    if (lineWidth > maxWidth) maxWidth = lineWidth;
    totalHeight += lineHeight;

    size.width = maxWidth;
    size.height = totalHeight;
    return size;
}

/**
 * @brief 创建渲染好的文本表面（RGBA8888）
 *
 * @param font    字库
 * @param text    UTF-8 文本
 * @param fgColor 前景色（墨点颜色）
 * @param bgColor 背景色；alpha 为 0（dbtext::kTransparent）时背景透明
 */
inline SDL_Surface* createTextSurface(const FontDesc& font, const char* text,
                                      const SDL_Color& fgColor,
                                      const SDL_Color& bgColor = kTransparent) {
    const TextSize size = measureText(font, text);
    if (size.width <= 0 || size.height <= 0) return nullptr;

    SDL_Surface* surface = SDL_CreateSurface(size.width, size.height, SDL_PIXELFORMAT_RGBA8888);
    if (surface == nullptr) return nullptr;

    // 先铺背景色（全透明时等价于清零）
    SDL_FillSurfaceRect(surface, nullptr,
                        SDL_MapSurfaceRGBA(surface, bgColor.r, bgColor.g, bgColor.b, bgColor.a));

    // 颜色值按目标表面的像素格式计算，避免依赖主机字节序
    const uint32_t colorValue = SDL_MapSurfaceRGBA(surface, fgColor.r, fgColor.g, fgColor.b, fgColor.a);

    const char* cursor = text;
    const char* end = text + std::strlen(text);

    int cx = 0;
    int cy = 0;
    int lineHeight = font.cellHeight;
    GlyphInfo scratch;

    while (cursor < end) {
        uint32_t codepoint = 0;
        if (detail::decodeUtf8(cursor, end, codepoint) <= 0) break;

        if (codepoint == '\n') {
            cy += lineHeight;
            lineHeight = font.cellHeight;
            cx = 0;
            continue;
        }
        if (codepoint == '\t') {
            cx += font.tabWidth != 0 ? font.tabWidth : font.cellWidth;
            continue;
        }
        if (codepoint == '\r') continue;

        const GlyphInfo* glyph = resolveGlyph(font, codepoint, scratch);
        if (glyph == nullptr) continue;

        for (uint32_t y = 0; y < glyph->height; ++y) {
            const int py = cy + static_cast<int>(y);
            if (py < 0 || py >= surface->h) break;

            uint32_t* row = reinterpret_cast<uint32_t*>(
                static_cast<uint8_t*>(surface->pixels) + static_cast<size_t>(py) * surface->pitch);

            for (uint32_t x = 0; x < glyph->width; ++x) {
                const int px = cx + static_cast<int>(x);
                if (px < 0 || px >= surface->w) break;
                if (!glyph->bitAt(y * glyph->width + x)) continue;
                row[px] = colorValue;
            }
        }

        if (static_cast<int>(glyph->height) > lineHeight) lineHeight = static_cast<int>(glyph->height);
        cx += static_cast<int>(glyph->width);
    }

    return surface;
}

inline SDL_Surface* createTextSurface(const FontDesc* font, const char* text,
                                      const SDL_Color& fgColor,
                                      const SDL_Color& bgColor = kTransparent) {
    if (font == nullptr) return nullptr;
    return createTextSurface(*font, text, fgColor, bgColor);
}

/**
 * @brief 渲染文本到 SDL 渲染器
 *
 * @param renderer 目标渲染器
 * @param font     字库
 * @param ux,uy    文本位置（未扣除 center 偏移）
 * @param fgColor  前景色（墨点颜色）
 * @param bgColor  背景色；alpha 为 0（dbtext::kTransparent）时背景透明
 * @param ptsize   缩放倍数（1.0f = 原始像素大小）
 * @param text     UTF-8 文本
 * @param center   锚点比例（0,0 = 左上，0.5,0.5 = 居中，1,1 = 右下）
 * @return SDL_Rect 实际绘制区域；失败时返回 {-1,-1,-1,-1}
 */
inline SDL_Rect paintText(SDL_Renderer* renderer, const FontDesc& font, int ux, int uy,
                          const SDL_Color& fgColor, const SDL_Color& bgColor,
                          float ptsize, const char* text,
                          const SDL_FPoint& center = {0.0f, 0.0f}) {
    SDL_Surface* surface = createTextSurface(font, text, fgColor, bgColor);
    if (surface == nullptr) return {-1, -1, -1, -1};

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        SDL_DestroySurface(surface);
        return {-1, -1, -1, -1};
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    SDL_Rect rect = {
        ux,
        uy,
        static_cast<int>(std::lround(surface->w * ptsize)),
        static_cast<int>(std::lround(surface->h * ptsize))
    };
    rect.x -= static_cast<int>(rect.w * center.x);
    rect.y -= static_cast<int>(rect.h * center.y);

    const SDL_FRect dst = {
        static_cast<float>(rect.x),
        static_cast<float>(rect.y),
        static_cast<float>(rect.w),
        static_cast<float>(rect.h)
    };
    SDL_RenderTexture(renderer, texture, nullptr, &dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
    return rect;
}

//! 只指定前景色（背景透明）
inline SDL_Rect paintText(SDL_Renderer* renderer, const FontDesc& font, int ux, int uy,
                          const SDL_Color& fgColor, float ptsize, const char* text,
                          const SDL_FPoint& center = {0.0f, 0.0f}) {
    return paintText(renderer, font, ux, uy, fgColor, kTransparent, ptsize, text, center);
}

inline SDL_Rect paintText(SDL_Renderer* renderer, FontID fontId, int ux, int uy,
                          const SDL_Color& fgColor, const SDL_Color& bgColor,
                          float ptsize, const char* text,
                          const SDL_FPoint& center = {0.0f, 0.0f}) {
    const FontDesc* font = findFont(fontId);
    if (font == nullptr) return {-1, -1, -1, -1};
    return paintText(renderer, *font, ux, uy, fgColor, bgColor, ptsize, text, center);
}

//! 只指定前景色（背景透明）
inline SDL_Rect paintText(SDL_Renderer* renderer, FontID fontId, int ux, int uy,
                          const SDL_Color& fgColor, float ptsize, const char* text,
                          const SDL_FPoint& center = {0.0f, 0.0f}) {
    return paintText(renderer, fontId, ux, uy, fgColor, kTransparent, ptsize, text, center);
}

namespace detail {

inline SDL_Rect paintTextFormatV(SDL_Renderer* renderer, const FontDesc& font, int ux, int uy,
                                 const SDL_Color& fgColor, const SDL_Color& bgColor, float ptsize,
                                 const SDL_FPoint& center, const char* format, va_list args) {
    char buffer[1024];

    const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
    if (written < 0) buffer[0] = '\0';

    return paintText(renderer, font, ux, uy, fgColor, bgColor, ptsize, buffer, center);
}

} // namespace detail

/**
 * @brief printf 风格的文本渲染（前景色 + 背景色）
 * @note 格式化结果最长为 1024 字节，超长会被截断
 */
inline SDL_Rect paintTextFormat(SDL_Renderer* renderer, const FontDesc& font, int ux, int uy,
                                SDL_Color fgColor, SDL_Color bgColor, float ptsize,
                                const SDL_FPoint& center, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const SDL_Rect result = detail::paintTextFormatV(renderer, font, ux, uy, fgColor, bgColor, ptsize, center, format, args);
    va_end(args);
    return result;
}

//! 只指定前景色（背景透明）
inline SDL_Rect paintTextFormat(SDL_Renderer* renderer, const FontDesc& font, int ux, int uy,
                                SDL_Color fgColor, float ptsize, const SDL_FPoint& center,
                                const char* format, ...) {
    va_list args;
    va_start(args, format);
    const SDL_Rect result = detail::paintTextFormatV(renderer, font, ux, uy, fgColor, kTransparent, ptsize, center, format, args);
    va_end(args);
    return result;
}

inline SDL_Rect paintTextFormat(SDL_Renderer* renderer, FontID fontId, int ux, int uy,
                                SDL_Color fgColor, SDL_Color bgColor, float ptsize,
                                const SDL_FPoint& center, const char* format, ...) {
    const FontDesc* font = findFont(fontId);
    if (font == nullptr) return {-1, -1, -1, -1};

    va_list args;
    va_start(args, format);
    const SDL_Rect result = detail::paintTextFormatV(renderer, *font, ux, uy, fgColor, bgColor, ptsize, center, format, args);
    va_end(args);
    return result;
}

//! 只指定前景色（背景透明）
inline SDL_Rect paintTextFormat(SDL_Renderer* renderer, FontID fontId, int ux, int uy,
                                SDL_Color fgColor, float ptsize, const SDL_FPoint& center,
                                const char* format, ...) {
    const FontDesc* font = findFont(fontId);
    if (font == nullptr) return {-1, -1, -1, -1};

    va_list args;
    va_start(args, format);
    const SDL_Rect result = detail::paintTextFormatV(renderer, *font, ux, uy, fgColor, kTransparent, ptsize, center, format, args);
    va_end(args);
    return result;
}

} // namespace dbtext

// ============================================================================
// 兼容层：v1 的全局接口（使用当前字库）
// 定义 DBTEXT_NO_LEGACY_API 可关闭
// ============================================================================
#ifndef DBTEXT_NO_LEGACY_API

//! @deprecated 请改用 dbtext::createTextSurface()
inline SDL_Surface* createDBTextSurface(const char* text, const SDL_Color& color) {
    return dbtext::createTextSurface(dbtext::currentFont(), text, color);
}

//! @deprecated 请改用 dbtext::paintText()
inline SDL_Rect paintDBText(SDL_Renderer* renderer, int ux, int uy, const SDL_Color& color,
                            float ptsize, const char* text,
                            const SDL_FPoint& center = {0.0f, 0.0f}) {
    const dbtext::FontDesc* font = dbtext::currentFont();
    if (font == nullptr) return {-1, -1, -1, -1};
    return dbtext::paintText(renderer, *font, ux, uy, color, ptsize, text, center);
}

//! @deprecated 请改用 dbtext::paintTextFormat()
inline SDL_Rect paintDBTextFormat(SDL_Renderer* renderer, int ux, int uy, SDL_Color color,
                                  float ptsize, const SDL_FPoint& center, const char* text, ...) {
    const dbtext::FontDesc* font = dbtext::currentFont();
    if (font == nullptr) return {-1, -1, -1, -1};

    char buffer[1024];

    va_list args;
    va_start(args, text);
    const int written = std::vsnprintf(buffer, sizeof(buffer), text, args);
    va_end(args);

    if (written < 0) buffer[0] = '\0';

    return dbtext::paintText(renderer, *font, ux, uy, color, ptsize, buffer, center);
}

#endif // DBTEXT_NO_LEGACY_API

// ============================================================================
// 点阵字体 → TTF 导出（可选功能，需要 SDL3_ttf）
// ============================================================================
#ifdef DBTEXT_ENABLE_TTF_GENERATION

#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <deque>
#include <utility>

namespace dbtext {
namespace ttf {

//! 每个点阵像素对应的字体单位数（<= 21 时，坐标差可用单字节短矢量表示）
inline constexpr int PIXELS_TO_UNITS = 20;

//! maxp.numGlyphs 为 uint16，字形数量上限
inline constexpr uint32_t MAX_GLYPHS = 65535;

/**
 * @brief 由字库描述推导出的 TTF 几何参数（一个单元格 == 1 em）
 */
struct Geometry {
    int cellHeight = 0;
    int unitsPerEm = 0;
    int ascender = 0;
    int descender = 0;
};

inline Geometry makeGeometry(const FontDesc& font) {
    Geometry geometry;
    geometry.cellHeight = font.cellHeight > 0 ? static_cast<int>(font.cellHeight) : 12;
    geometry.unitsPerEm = geometry.cellHeight * PIXELS_TO_UNITS;
    geometry.ascender = geometry.unitsPerEm;
    geometry.descender = 0;
    return geometry;
}

namespace detail {

inline void putU8(std::vector<uint8_t>& out, uint32_t v) { out.push_back(static_cast<uint8_t>(v & 0xFFu)); }
inline void putU16(std::vector<uint8_t>& out, uint32_t v) { putU8(out, v >> 8); putU8(out, v); }
inline void putS16(std::vector<uint8_t>& out, int32_t v) { putU16(out, static_cast<uint32_t>(static_cast<uint16_t>(static_cast<int16_t>(v)))); }
inline void putU32(std::vector<uint8_t>& out, uint32_t v) { putU16(out, v >> 16); putU16(out, v & 0xFFFFu); }
inline void putTag(std::vector<uint8_t>& out, const char* tag) { for (int i = 0; i < 4; ++i) putU8(out, static_cast<uint8_t>(tag[i])); }
inline void padTo4(std::vector<uint8_t>& out) { while ((out.size() & 3u) != 0u) out.push_back(0); }

//! sfnt 表校验和（长度不足 4 字节的部分按 0 补齐）
inline uint32_t checksum32(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i += 4) {
        uint32_t word = 0;
        for (size_t j = 0; j < 4; ++j) {
            word <<= 8;
            if (i + j < len) word |= data[i + j];
        }
        sum += word;
    }
    return sum;
}

//! 一个已合并的墨点矩形（单位：字体单位，y 轴向上，y = 0 为基线）
struct Rect { int x0, x1, y0, y1; };

//! 单个字形的轮廓统计信息
struct GlyphStats {
    bool empty = true;
    int points = 0;
    int contours = 0;
    int xMin = 0, yMin = 0, xMax = 0, yMax = 0;
};

//! 生成过程中的可复用缓冲区
struct Scratch {
    std::vector<Rect> rects;
    std::vector<Rect> open;
    std::vector<Rect> nextOpen;
    std::vector<uint8_t> flags;
    std::vector<uint8_t> xData;
    std::vector<uint8_t> yData;
};

//! 1 = 墨点（v2 位约定）
inline bool isInk(const GlyphInfo& glyph, int x, int y) {
    return glyph.bitAt(static_cast<uint32_t>(y * glyph.width + x));
}

//! 把一行的连续墨点合并成矩形，并把上下相邻的同列矩形纵向合并
inline void extractRects(const Geometry& geometry, const GlyphInfo& glyph,
                         std::vector<Rect>& rects, std::vector<Rect>& open,
                         std::vector<Rect>& nextOpen) {
    rects.clear();
    open.clear();

    const int width = glyph.width;
    const int height = glyph.height;
    if (width <= 0 || height <= 0) return;

    for (int y = 0; y < height; ++y) {
        const int yBottom = (geometry.cellHeight - y - 1) * PIXELS_TO_UNITS;
        nextOpen.clear();

        int x = 0;
        while (x < width) {
            if (!isInk(glyph, x, y)) { ++x; continue; }
            const int colBegin = x;
            while (x < width && isInk(glyph, x, y)) ++x;

            const int x0 = colBegin * PIXELS_TO_UNITS;
            const int x1 = x * PIXELS_TO_UNITS;

            bool merged = false;
            for (size_t i = 0; i < open.size(); ++i) {
                if (open[i].x0 == x0 && open[i].x1 == x1) {
                    open[i].y0 = yBottom;
                    nextOpen.push_back(open[i]);
                    open.erase(open.begin() + static_cast<std::ptrdiff_t>(i));
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                nextOpen.push_back(Rect{ x0, x1, yBottom, yBottom + PIXELS_TO_UNITS });
            }
        }

        for (size_t i = 0; i < open.size(); ++i) rects.push_back(open[i]);
        open.swap(nextOpen);
    }

    for (size_t i = 0; i < open.size(); ++i) rects.push_back(open[i]);
}

//! 生成一个字形在 glyf 表中的数据（空字形不写入任何字节）
inline void buildGlyph(const Geometry& geometry, const GlyphInfo& glyph,
                       std::vector<uint8_t>& glyf, GlyphStats& stats, Scratch& scratch) {
    std::vector<Rect>& rects = scratch.rects;
    std::vector<uint8_t>& flags = scratch.flags;
    std::vector<uint8_t>& xData = scratch.xData;
    std::vector<uint8_t>& yData = scratch.yData;

    stats = GlyphStats{};
    extractRects(geometry, glyph, rects, scratch.open, scratch.nextOpen);
    if (rects.empty()) return;

    int xMin = rects[0].x0, xMax = rects[0].x1;
    int yMin = rects[0].y0, yMax = rects[0].y1;
    for (const Rect& r : rects) {
        if (r.x0 < xMin) xMin = r.x0;
        if (r.x1 > xMax) xMax = r.x1;
        if (r.y0 < yMin) yMin = r.y0;
        if (r.y1 > yMax) yMax = r.y1;
    }

    stats.empty = false;
    stats.contours = static_cast<int>(rects.size());
    stats.points = stats.contours * 4;
    stats.xMin = xMin;
    stats.yMin = yMin;
    stats.xMax = xMax;
    stats.yMax = yMax;

    putS16(glyf, stats.contours);
    putS16(glyf, xMin);
    putS16(glyf, yMin);
    putS16(glyf, xMax);
    putS16(glyf, yMax);

    int endPoint = 0;
    for (int i = 0; i < stats.contours; ++i) {
        endPoint += 4;
        putU16(glyf, static_cast<uint32_t>(endPoint - 1));
    }
    putU16(glyf, 0);

    flags.clear();
    xData.clear();
    yData.clear();

    int px = 0, py = 0;
    auto emitPoint = [&](int x, int y) {
        const int dx = x - px;
        const int dy = y - py;
        uint8_t flag = 0x01;                        // ON_CURVE_POINT

        if (dx == 0) {
            flag |= 0x10;
        } else if (dx >= -255 && dx <= 255) {
            flag |= 0x02;
            if (dx > 0) flag |= 0x10;
            xData.push_back(static_cast<uint8_t>(dx < 0 ? -dx : dx));
        } else {
            putS16(xData, dx);
        }

        if (dy == 0) {
            flag |= 0x20;
        } else if (dy >= -255 && dy <= 255) {
            flag |= 0x04;
            if (dy > 0) flag |= 0x20;
            yData.push_back(static_cast<uint8_t>(dy < 0 ? -dy : dy));
        } else {
            putS16(yData, dy);
        }

        flags.push_back(flag);
        px = x;
        py = y;
    };

    for (const Rect& r : rects) {
        emitPoint(r.x0, r.y1);
        emitPoint(r.x1, r.y1);
        emitPoint(r.x1, r.y0);
        emitPoint(r.x0, r.y0);
    }

    glyf.insert(glyf.end(), flags.begin(), flags.end());
    glyf.insert(glyf.end(), xData.begin(), xData.end());
    glyf.insert(glyf.end(), yData.begin(), yData.end());
    padTo4(glyf);
}

} // namespace detail

/**
 * @brief 把点阵字库描述转换成完整的 TTF 字节流
 * @return TTF 数据；失败时为空
 */
inline std::vector<uint8_t> buildTTF(const FontDesc& font) {
    std::vector<uint8_t> ttf;
    if (!font.valid()) return ttf;

    const Geometry geometry = makeGeometry(font);

    uint32_t numGlyphs = font.maxCodepoint + 1u;
    if (numGlyphs > MAX_GLYPHS) numGlyphs = MAX_GLYPHS;
    if (numGlyphs <= 2u) return ttf;

    std::vector<uint8_t> glyf;
    glyf.reserve(static_cast<size_t>(numGlyphs) * 128);

    std::vector<uint32_t> locaOffsets(numGlyphs + 1, 0);
    std::vector<uint8_t> hmtx;
    hmtx.reserve(static_cast<size_t>(numGlyphs) * 4);

    int maxPoints = 0;
    int maxContours = 0;
    int advanceMax = 0;
    int bboxXMin = 0, bboxYMin = 0, bboxXMax = 0, bboxYMax = 0;
    bool hasBox = false;

    detail::GlyphStats stats;
    detail::Scratch scratch;
    GlyphInfo glyph;

    for (uint32_t g = 0; g < numGlyphs; ++g) {
        locaOffsets[g] = static_cast<uint32_t>(glyf.size());

        int advance = 0;
        if (font.findGlyph(g, glyph) && glyph.width > 0 && glyph.height > 0) {
            detail::buildGlyph(geometry, glyph, glyf, stats, scratch);
            advance = glyph.width * PIXELS_TO_UNITS;
        } else {
            stats = detail::GlyphStats{};
            advance = 0;
        }

        detail::putU16(hmtx, static_cast<uint32_t>(advance));
        detail::putS16(hmtx, stats.empty ? 0 : stats.xMin);
        if (advance > advanceMax) advanceMax = advance;

        if (!stats.empty) {
            if (stats.points > maxPoints) maxPoints = stats.points;
            if (stats.contours > maxContours) maxContours = stats.contours;
            if (!hasBox) {
                bboxXMin = stats.xMin; bboxYMin = stats.yMin;
                bboxXMax = stats.xMax; bboxYMax = stats.yMax;
                hasBox = true;
            } else {
                if (stats.xMin < bboxXMin) bboxXMin = stats.xMin;
                if (stats.yMin < bboxYMin) bboxYMin = stats.yMin;
                if (stats.xMax > bboxXMax) bboxXMax = stats.xMax;
                if (stats.yMax > bboxYMax) bboxYMax = stats.yMax;
            }
        }
    }
    locaOffsets[numGlyphs] = static_cast<uint32_t>(glyf.size());

    const uint16_t lastMapped = static_cast<uint16_t>(std::min<uint32_t>(numGlyphs - 1u, 0xFFFEu));
    const uint16_t usWinDescent = static_cast<uint16_t>(bboxYMin < 0 ? -bboxYMin : 0);

    // ---------------- head ----------------
    std::vector<uint8_t> head;
    detail::putU32(head, 0x00010000);
    detail::putU32(head, 0x00010000);
    detail::putU32(head, 0);
    detail::putU32(head, 0x5F0F3CF5);
    detail::putU16(head, 0x000B);
    detail::putU16(head, static_cast<uint32_t>(geometry.unitsPerEm));
    detail::putU32(head, 0); detail::putU32(head, 0);
    detail::putU32(head, 0); detail::putU32(head, 0);
    detail::putS16(head, bboxXMin);
    detail::putS16(head, bboxYMin);
    detail::putS16(head, bboxXMax);
    detail::putS16(head, bboxYMax);
    detail::putU16(head, 0);
    detail::putU16(head, 8);
    detail::putS16(head, 2);
    detail::putS16(head, 1);
    detail::putS16(head, 0);

    // ---------------- hhea ----------------
    std::vector<uint8_t> hhea;
    detail::putU32(hhea, 0x00010000);
    detail::putS16(hhea, geometry.ascender);
    detail::putS16(hhea, -geometry.descender);
    detail::putS16(hhea, 0);
    detail::putU16(hhea, static_cast<uint32_t>(advanceMax));
    detail::putS16(hhea, 0);
    detail::putS16(hhea, 0);
    detail::putS16(hhea, advanceMax);
    detail::putS16(hhea, 1);
    detail::putS16(hhea, 0);
    detail::putS16(hhea, 0);
    detail::putS16(hhea, 0); detail::putS16(hhea, 0); detail::putS16(hhea, 0); detail::putS16(hhea, 0);
    detail::putS16(hhea, 0);
    detail::putU16(hhea, numGlyphs);

    // ---------------- maxp ----------------
    std::vector<uint8_t> maxp;
    detail::putU32(maxp, 0x00010000);
    detail::putU16(maxp, numGlyphs);
    detail::putU16(maxp, static_cast<uint32_t>(maxPoints));
    detail::putU16(maxp, static_cast<uint32_t>(maxContours));
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 2);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);
    detail::putU16(maxp, 0);

    // ---------------- loca（长格式） ----------------
    std::vector<uint8_t> loca;
    loca.reserve(static_cast<size_t>(numGlyphs + 1) * 4);
    for (uint32_t i = 0; i <= numGlyphs; ++i) detail::putU32(loca, locaOffsets[i]);

    // ---------------- cmap（format 4，码位 == 字形索引） ----------------
    std::vector<uint8_t> cmapSub;
    detail::putU16(cmapSub, 4);
    detail::putU16(cmapSub, 32);
    detail::putU16(cmapSub, 0);
    detail::putU16(cmapSub, 4);
    detail::putU16(cmapSub, 4);
    detail::putU16(cmapSub, 1);
    detail::putU16(cmapSub, 0);
    detail::putU16(cmapSub, lastMapped);
    detail::putU16(cmapSub, 0xFFFF);
    detail::putU16(cmapSub, 0);
    detail::putU16(cmapSub, 0x0001);
    detail::putU16(cmapSub, 0xFFFF);
    detail::putS16(cmapSub, 0);
    detail::putS16(cmapSub, 1);
    detail::putU16(cmapSub, 0);
    detail::putU16(cmapSub, 0);

    std::vector<uint8_t> cmap;
    detail::putU16(cmap, 0);
    detail::putU16(cmap, 2);
    detail::putU16(cmap, 0); detail::putU16(cmap, 3); detail::putU32(cmap, 20);
    detail::putU16(cmap, 3); detail::putU16(cmap, 1); detail::putU32(cmap, 20);
    cmap.insert(cmap.end(), cmapSub.begin(), cmapSub.end());

    // ---------------- name ----------------
    struct NameString { uint16_t id; const char* utf8; };
    const NameString names[] = {
        { 1, "DebugBitmapFont" },
        { 2, "Regular" },
        { 3, "DebugBitmapFont Regular; 1.0" },
        { 4, "DebugBitmapFont Regular" },
        { 6, "DebugBitmapFont-Regular" },
    };
    const uint16_t nameCount = static_cast<uint16_t>(sizeof(names) / sizeof(names[0]));

    std::vector<uint8_t> name;
    detail::putU16(name, 0);
    detail::putU16(name, nameCount);
    detail::putU16(name, static_cast<uint32_t>(6 + 12 * nameCount));

    std::vector<uint8_t> nameStrings;
    for (const NameString& ns : names) {
        const size_t len = std::strlen(ns.utf8);
        detail::putU16(name, 3);
        detail::putU16(name, 1);
        detail::putU16(name, 0x0409);
        detail::putU16(name, ns.id);
        detail::putU16(name, static_cast<uint32_t>(len * 2));
        detail::putU16(name, static_cast<uint32_t>(nameStrings.size()));
        for (size_t i = 0; i < len; ++i) {
            nameStrings.push_back(0);
            nameStrings.push_back(static_cast<uint8_t>(ns.utf8[i]));
        }
    }
    name.insert(name.end(), nameStrings.begin(), nameStrings.end());

    // ---------------- post（3.0） ----------------
    std::vector<uint8_t> post;
    detail::putU32(post, 0x00030000);
    detail::putU32(post, 0);
    detail::putS16(post, -100);
    detail::putS16(post, 50);
    detail::putU32(post, 0);
    detail::putU32(post, 0); detail::putU32(post, 0); detail::putU32(post, 0); detail::putU32(post, 0);

    // ---------------- OS/2（version 4） ----------------
    std::vector<uint8_t> os2;
    detail::putU16(os2, 4);
    detail::putS16(os2, 600);
    detail::putU16(os2, 400);
    detail::putU16(os2, 5);
    detail::putU16(os2, 0);
    detail::putS16(os2, 650); detail::putS16(os2, 600); detail::putS16(os2, 0); detail::putS16(os2, 75);
    detail::putS16(os2, 650); detail::putS16(os2, 600); detail::putS16(os2, 0); detail::putS16(os2, 350);
    detail::putS16(os2, 50); detail::putS16(os2, 250);
    detail::putS16(os2, 0);
    for (int i = 0; i < 10; ++i) detail::putU8(os2, 0);
    detail::putU32(os2, 0x00000001);
    detail::putU32(os2, 0x08000000);
    detail::putU32(os2, 0);
    detail::putU32(os2, 0);
    detail::putTag(os2, "DBTF");
    detail::putU16(os2, 0x0040);
    detail::putU16(os2, 0x0001);
    detail::putU16(os2, lastMapped);
    detail::putS16(os2, geometry.ascender);
    detail::putS16(os2, -geometry.descender);
    detail::putS16(os2, 0);
    detail::putU16(os2, static_cast<uint32_t>(geometry.ascender));
    detail::putU16(os2, usWinDescent);
    detail::putU32(os2, 0x00000001);
    detail::putU32(os2, 0);
    detail::putS16(os2, 6 * PIXELS_TO_UNITS);
    detail::putS16(os2, 7 * PIXELS_TO_UNITS);
    detail::putU16(os2, 0);
    detail::putU16(os2, 32);
    detail::putU16(os2, 0);

    // ---------------- 组装 sfnt ----------------
    struct TableRef { const char* tag; const std::vector<uint8_t>* data; };
    TableRef tables[] = {
        { "head", &head },
        { "hhea", &hhea },
        { "maxp", &maxp },
        { "hmtx", &hmtx },
        { "cmap", &cmap },
        { "glyf", &glyf },
        { "loca", &loca },
        { "name", &name },
        { "post", &post },
        { "OS/2", &os2 },
    };
    const size_t tableCount = sizeof(tables) / sizeof(tables[0]);

    std::sort(std::begin(tables), std::end(tables),
              [](const TableRef& a, const TableRef& b) { return std::strcmp(a.tag, b.tag) < 0; });

    const uint16_t numTablesValue = static_cast<uint16_t>(tableCount);
    uint16_t entrySelector = 0;
    while ((1u << (entrySelector + 1)) <= numTablesValue) ++entrySelector;
    const uint16_t searchRange = static_cast<uint16_t>(16u << entrySelector);
    const uint16_t rangeShift = static_cast<uint16_t>(numTablesValue * 16u - searchRange);

    std::vector<uint32_t> tableOffset(tableCount, 0);
    uint32_t cursor = static_cast<uint32_t>(12 + 16 * numTablesValue);
    for (size_t i = 0; i < tableCount; ++i) {
        tableOffset[i] = cursor;
        cursor += static_cast<uint32_t>((tables[i].data->size() + 3u) & ~size_t(3u));
    }

    ttf.clear();
    ttf.reserve(cursor);
    detail::putU32(ttf, 0x00010000);
    detail::putU16(ttf, numTablesValue);
    detail::putU16(ttf, searchRange);
    detail::putU16(ttf, entrySelector);
    detail::putU16(ttf, rangeShift);

    for (size_t i = 0; i < tableCount; ++i) {
        const std::vector<uint8_t>& data = *tables[i].data;
        detail::putTag(ttf, tables[i].tag);
        detail::putU32(ttf, detail::checksum32(data.data(), data.size()));
        detail::putU32(ttf, tableOffset[i]);
        detail::putU32(ttf, static_cast<uint32_t>(data.size()));
    }

    for (size_t i = 0; i < tableCount; ++i) {
        ttf.insert(ttf.end(), tables[i].data->begin(), tables[i].data->end());
        detail::padTo4(ttf);
    }

    uint32_t headOffset = 0;
    for (size_t i = 0; i < tableCount; ++i) {
        if (std::strcmp(tables[i].tag, "head") == 0) headOffset = tableOffset[i];
    }
    const uint32_t adjustment = 0xB1B0AFBAu - detail::checksum32(ttf.data(), ttf.size());
    ttf[headOffset + 8]  = static_cast<uint8_t>((adjustment >> 24) & 0xFFu);
    ttf[headOffset + 9]  = static_cast<uint8_t>((adjustment >> 16) & 0xFFu);
    ttf[headOffset + 10] = static_cast<uint8_t>((adjustment >> 8) & 0xFFu);
    ttf[headOffset + 11] = static_cast<uint8_t>(adjustment & 0xFFu);

    return ttf;
}

/**
 * @brief 获取某个字库的 TTF 字节流（每个字库只生成一次）
 */
inline const std::vector<uint8_t>& getFontData(const FontDesc& font) {
    // deque：元素地址稳定，TTF_OpenFontIO() 持有内存时不会被后续插入失效
    static std::deque<std::pair<const FontDesc*, std::vector<uint8_t>>> cache;
    for (const auto& entry : cache) {
        if (entry.first == &font) return entry.second;
    }
    cache.emplace_back(&font, buildTTF(font));
    return cache.back().second;
}

/**
 * @brief 用内存中的 TTF 打开 SDL_ttf 字体
 *
 * @param font   点阵字库描述
 * @param ptsize 点大小（0 表示与点阵 1:1，即 font.cellHeight）
 * @return TTF_Font*，失败返回 nullptr（可用 SDL_GetError() 查看原因）
 */
inline TTF_Font* openFont(const FontDesc& font, float ptsize = 0.0f) {
    const std::vector<uint8_t>& data = getFontData(font);
    if (data.empty()) {
        SDL_SetError("dbtext::ttf: 无法把点阵字体转换为 TTF");
        return nullptr;
    }

    SDL_IOStream* io = SDL_IOFromMem(const_cast<uint8_t*>(data.data()), data.size());
    if (io == nullptr) return nullptr;

    const float targetSize = ptsize > 0.0f ? ptsize : static_cast<float>(font.cellHeight);
    TTF_Font* ttfFont = TTF_OpenFontIO(io, true, targetSize);
    if (ttfFont == nullptr) return nullptr;

    TTF_SetFontHinting(ttfFont, TTF_HINTING_NONE);
    return ttfFont;
}

/**
 * @brief 把字库转换成 TTF 并保存到磁盘（调试用）
 */
inline bool saveToFile(const FontDesc& font, const char* path) {
    const std::vector<uint8_t>& data = getFontData(font);
    if (data.empty()) return false;

    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return false;

    const size_t written = std::fwrite(data.data(), 1, data.size(), file);
    std::fclose(file);
    return written == data.size();
}

//! 兼容旧接口：使用当前字库
inline TTF_Font* openDebugFont(float ptsize = 0.0f) {
    const FontDesc* font = currentFont();
    if (font == nullptr) return nullptr;
    return openFont(*font, ptsize);
}

} // namespace ttf
} // namespace dbtext

#endif // DBTEXT_ENABLE_TTF_GENERATION

#endif // __INC_DEBUGTEXT_
