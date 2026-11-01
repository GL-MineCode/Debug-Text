#define NOMINMAX

#include "FontBuilder.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

/**
 * @brief 判断某个像素是否为墨点
 *
 * TTF_RenderGlyph_Solid() 返回的是 8 位调色板表面：索引 0 = 透明背景（colorkey），1 = 前景色，
 * 直接读索引最可靠。若格式不是 8 位则回退到 alpha 判定。
 */
inline bool isInkPixel(const SDL_Surface* surface, int x, int y) {
    const uint8_t* row = static_cast<const uint8_t*>(surface->pixels) + static_cast<size_t>(y) * surface->pitch;

    if (SDL_BITSPERPIXEL(surface->format) == 8) {
        return row[x] != 0u;
    }

    const uint32_t pixel = *reinterpret_cast<const uint32_t*>(row + static_cast<size_t>(x) * 4u);
    uint8_t r = 0, g = 0, b = 0, a = 0;
    SDL_GetRGBA(pixel, SDL_GetPixelFormatDetails(surface->format), nullptr, &r, &g, &b, &a);
    return a > 127u;
}

inline void setBit(std::vector<uint8_t>& bytes, uint32_t index) {
    bytes[index >> 3] |= static_cast<uint8_t>(0x80u >> (index & 7u));
}

inline uint32_t popcount32(uint32_t value) {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<uint32_t>(__builtin_popcount(value));
#else
    value = value - ((value >> 1) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2) & 0x33333333u);
    value = (value + (value >> 4)) & 0x0F0F0F0Fu;
    return (value * 0x01010101u) >> 24;
#endif
}

//! 把字节流按小端打成 32 位词（与 DebugText.hpp 的 GlyphInfo::byteAt 约定一致）
std::vector<uint32_t> packBytes(const std::vector<uint8_t>& bytes) {
    std::vector<uint32_t> words((bytes.size() + 3u) / 4u, 0u);
    for (size_t i = 0; i < bytes.size(); ++i) {
        words[i >> 2] |= static_cast<uint32_t>(bytes[i]) << (8u * static_cast<uint32_t>(i & 3u));
    }
    return words;
}

//! counts[b] = 第 b 块（256 项）之前的 1 的个数；bits 必须已补齐到 blocks*8 个词
void buildBlockCounts(const std::vector<uint32_t>& bits, uint32_t blocks, std::vector<uint32_t>& counts) {
    counts.assign(blocks, 0u);
    uint32_t total = 0;
    for (uint32_t block = 0; block < blocks; ++block) {
        counts[block] = total;
        for (uint32_t word = 0; word < 8u; ++word) {
            total += popcount32(bits[static_cast<size_t>(block) * 8u + word]);
        }
    }
}

struct RawGlyph {
    uint32_t codepoint = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    bool fullWidth = false;
    std::vector<uint8_t> bits;   // 行宽 = width，1 = 墨点
};

std::string fileNameOf(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

void report(const FontBuildOptions& options, const ProgressInfo& info) {
    if (options.onProgress != nullptr) options.onProgress(options.progressUser, info);
}

} // namespace

bool buildFontFromTTF(const FontBuildOptions& options, PackedFont& out, std::string& error) {
    out = PackedFont{};

    if (options.ttfPath.empty()) {
        error = "未指定 TTF 路径";
        return false;
    }

    const int requestSize = options.ptsize > 0 ? options.ptsize : 12;
    TTF_Font* font = TTF_OpenFont(options.ttfPath.c_str(), static_cast<float>(requestSize));
    if (font == nullptr) {
        error = "无法打开字体文件 " + options.ttfPath + "（" + SDL_GetError() + "）";
        return false;
    }
    TTF_SetFontHinting(font, TTF_HINTING_NONE);
    TTF_SetFontKerning(font, false);
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);

    const int measuredSize = static_cast<int>(std::lround(TTF_GetFontSize(font)));
    const int pixelSize = options.ptsize > 0 ? options.ptsize : (measuredSize > 0 ? measuredSize : requestSize);

    const int cellWidth = options.cellWidth > 0 ? options.cellWidth : pixelSize;
    int cellHeight = options.cellHeight > 0 ? options.cellHeight : pixelSize;
    const int halfWidth = std::max(1, cellWidth / 2);

    // ---------------- 扫描码点覆盖 ----------------
    std::vector<uint32_t> codepoints;
    const uint32_t scanMax = options.maxCodepoint;
    for (uint32_t codepoint = 0; codepoint <= scanMax; ++codepoint) {
        if (TTF_FontHasGlyph(font, codepoint)) codepoints.push_back(codepoint);
        if ((codepoint & 0x1FFFu) == 0u) {
            ProgressInfo info;
            info.percent = static_cast<int>((static_cast<uint64_t>(codepoint) * 40u) / (static_cast<uint64_t>(scanMax) + 1u));
            info.codepoint = codepoint;
            report(options, info);
        }
    }

    if (codepoints.empty()) {
        error = "字体中没有任何可用字形";
        TTF_CloseFont(font);
        return false;
    }

    // ---------------- 栅格化 ----------------
    std::vector<RawGlyph> glyphs;
    glyphs.reserve(codepoints.size());

    int maxWidth = 0;
    int maxHeight = 0;
    uint64_t v1BitmapBytes = 0;

    for (size_t i = 0; i < codepoints.size(); ++i) {
        const uint32_t codepoint = codepoints[i];

        int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
        const bool hasMetrics = TTF_GetGlyphMetrics(font, codepoint, &minx, &maxx, &miny, &maxy, &advance);

        SDL_Surface* surface = TTF_RenderGlyph_Solid(font, codepoint, SDL_Color{ 0, 0, 0, 255 });
        if (surface == nullptr) continue;

        RawGlyph glyph;
        glyph.codepoint = codepoint;
        glyph.width = static_cast<uint16_t>(std::max(0, surface->w));
        glyph.height = static_cast<uint16_t>(std::max(0, surface->h));
        glyph.fullWidth = hasMetrics ? (advance > halfWidth) : (surface->w > halfWidth);

        if (glyph.width > 0 && glyph.height > 0) {
            glyph.bits.assign((static_cast<size_t>(glyph.width) * glyph.height + 7u) / 8u, 0u);
            for (int y = 0; y < glyph.height; ++y) {
                for (int x = 0; x < glyph.width; ++x) {
                    if (!isInkPixel(surface, x, y)) continue;
                    setBit(glyph.bits, static_cast<uint32_t>(y) * glyph.width + static_cast<uint32_t>(x));
                }
            }
        }

        SDL_DestroySurface(surface);

        maxWidth = std::max<int>(maxWidth, glyph.width);
        maxHeight = std::max<int>(maxHeight, glyph.height);
        v1BitmapBytes += (static_cast<uint64_t>(glyph.width) * glyph.height + 7u) / 8u;

        glyphs.push_back(std::move(glyph));

        if ((i & 0xFFu) == 0u) {
            ProgressInfo info;
            info.percent = 40 + static_cast<int>((static_cast<uint64_t>(i) * 60u) / std::max<size_t>(1u, codepoints.size()));
            info.codepoint = codepoint;
            if (!glyphs.empty() && !glyphs.back().bits.empty()) {
                info.bits = glyphs.back().bits.data();
                info.width = glyphs.back().width;
                info.height = glyphs.back().height;
            }
            report(options, info);
        }
    }

    if (glyphs.empty()) {
        error = "所有字形的栅格化都失败了";
        TTF_CloseFont(font);
        return false;
    }

    // 若大量字形都比单元格高，直接抬升行高（否则会全部落到 extras 里）
    uint32_t tallerCount = 0;
    for (const RawGlyph& glyph : glyphs) {
        if (static_cast<int>(glyph.height) > cellHeight) ++tallerCount;
    }
    if (tallerCount * 20u > glyphs.size() && maxHeight > cellHeight) {
        if (options.verbose) {
            std::printf("  [提示] %u 个字形高于 %dpx，行高提升为 %dpx\n", tallerCount, cellHeight, maxHeight);
        }
        cellHeight = maxHeight;
    }

    // ---------------- 打包 ----------------
    out.id = options.id;
    out.styleName = "Regular";
    out.copyright = options.copyright;
    out.licenseName = options.licenseName;
    out.sourceFile = fileNameOf(options.ttfPath);
    out.sourceVersion = options.sourceVersion;
    out.cellWidth = static_cast<uint32_t>(cellWidth);
    out.cellHeight = static_cast<uint32_t>(cellHeight);
    out.halfWidth = static_cast<uint32_t>(halfWidth);
    out.tabWidth = static_cast<uint32_t>(cellWidth * std::max(1, options.tabColumns));
    out.halfRecordBytes = (static_cast<uint32_t>(halfWidth) * static_cast<uint32_t>(cellHeight) + 7u) / 8u;
    out.fullRecordBytes = (static_cast<uint32_t>(cellWidth) * static_cast<uint32_t>(cellHeight) + 7u) / 8u;
    out.glyphCount = static_cast<uint32_t>(glyphs.size());
    out.maxCodepoint = glyphs.back().codepoint;
    out.maxGlyphWidth = maxWidth;
    out.maxGlyphHeight = maxHeight;

    const char* family = TTF_GetFontFamilyName(font);
    const char* style = TTF_GetFontStyleName(font);
    out.displayName = options.displayName.empty() ? (family != nullptr ? family : options.id) : options.displayName;
    if (style != nullptr && style[0] != '\0') out.styleName = style;

    std::vector<uint8_t> atlasBytes;
    std::vector<uint8_t> extraBytes;
    std::vector<uint32_t> extraOffsets;
    std::vector<uint32_t> extraDimsRaw;

    const uint32_t fullWidthWordCount = ((out.glyphCount >> 8) + 1u) * 8u;
    std::vector<uint32_t> fullWidthBits(fullWidthWordCount, 0u);
    const uint32_t extraWordCount = ((out.glyphCount >> 8) + 1u) * 8u;
    std::vector<uint32_t> extraBits(extraWordCount, 0u);

    for (uint32_t index = 0; index < glyphs.size(); ++index) {
        const RawGlyph& glyph = glyphs[index];

        if (glyph.fullWidth) {
            fullWidthBits[index >> 5] |= 1u << (index & 31u);
            ++out.fullGlyphCount;
        } else {
            ++out.halfGlyphCount;
        }

        const bool hasInk = std::any_of(glyph.bits.begin(), glyph.bits.end(),
                                        [](uint8_t value) { return value != 0u; });
        if (!hasInk) ++out.emptyGlyphCount;

        const uint32_t classWidth = glyph.fullWidth ? out.cellWidth : out.halfWidth;
        const bool overflow = !glyph.bits.empty()
                           && (static_cast<uint32_t>(glyph.width) > classWidth
                               || static_cast<uint32_t>(glyph.height) > out.cellHeight);

        // 无论是否超格，atlas 里都为该字形保留一个槽位：
        // 这样「偏移 = fullRecordBytes*fullBefore + halfRecordBytes*(index-fullBefore」始终成立。
        const uint32_t recordBytes = glyph.fullWidth ? out.fullRecordBytes : out.halfRecordBytes;
        std::vector<uint8_t> record(recordBytes, 0u);
        if (!overflow && !glyph.bits.empty()) {
            for (uint32_t y = 0; y < glyph.height; ++y) {
                for (uint32_t x = 0; x < glyph.width; ++x) {
                    const uint32_t sourceIndex = y * glyph.width + x;
                    if (((glyph.bits[sourceIndex >> 3] >> (7u - (sourceIndex & 7u))) & 1u) == 0u) continue;
                    setBit(record, y * classWidth + x);
                }
            }
        }
        atlasBytes.insert(atlasBytes.end(), record.begin(), record.end());

        if (overflow) {
            extraBits[index >> 5] |= 1u << (index & 31u);
            extraDimsRaw.push_back(static_cast<uint32_t>(glyph.width) | (static_cast<uint32_t>(glyph.height) << 8));
            extraOffsets.push_back(static_cast<uint32_t>(extraBytes.size()));
            extraBytes.insert(extraBytes.end(), glyph.bits.begin(), glyph.bits.end());
        }
    }

    // 码点存在性位图
    const uint32_t presenceBlocks = (out.maxCodepoint >> 8) + 1u;
    out.presenceBits.assign(static_cast<size_t>(presenceBlocks) * 8u, 0u);
    for (const RawGlyph& glyph : glyphs) {
        out.presenceBits[glyph.codepoint >> 5] |= 1u << (glyph.codepoint & 31u);
    }
    buildBlockCounts(out.presenceBits, presenceBlocks, out.presenceBlocks);

    out.fullWidthBits = std::move(fullWidthBits);
    buildBlockCounts(out.fullWidthBits, (out.glyphCount >> 8) + 1u, out.fullWidthBlocks);

    if (!extraOffsets.empty()) {
        out.extraCount = static_cast<uint32_t>(extraOffsets.size());
        out.extraBits = std::move(extraBits);
        buildBlockCounts(out.extraBits, (out.glyphCount >> 8) + 1u, out.extraBlocks);
        out.extraDims.reserve(extraDimsRaw.size());
        for (uint32_t dims : extraDimsRaw) out.extraDims.push_back(static_cast<uint16_t>(dims & 0xFFFFu));
        out.extraOffsets = std::move(extraOffsets);
        out.extraData = packBytes(extraBytes);
    } else {
        out.extraBits.assign(extraWordCount, 0u);
        buildBlockCounts(out.extraBits, (out.glyphCount >> 8) + 1u, out.extraBlocks);
        out.extraCount = 0;
    }

    out.atlas = packBytes(atlasBytes);

    out.v2Bytes = 4ull * (out.presenceBits.size() + out.presenceBlocks.size()
                        + out.fullWidthBits.size() + out.fullWidthBlocks.size()
                        + out.atlas.size()
                        + out.extraBits.size() + out.extraBlocks.size()
                        + out.extraData.size())
                + 2ull * out.extraDims.size()
                + 4ull * out.extraOffsets.size();

    out.v1Bytes = 4ull + 4ull * (out.maxCodepoint + 1ull) + 2ull * (out.maxCodepoint + 1ull) + v1BitmapBytes;

    ProgressInfo done;
    done.percent = 100;
    done.codepoint = out.maxCodepoint;
    report(options, done);

    TTF_CloseFont(font);
    return true;
}

void printFontReport(const PackedFont& font) {
    std::printf("[%s]\n", font.id.c_str());
    std::printf("  名称      : %s / %s（源 %s %s）\n",
                font.displayName.c_str(), font.styleName.c_str(),
                font.sourceFile.c_str(), font.sourceVersion.c_str());
    std::printf("  度量      : 全角 %upx / 半角 %upx / 行高 %upx / Tab %upx\n",
                font.cellWidth, font.halfWidth, font.cellHeight, font.tabWidth);
    std::printf("  字形      : %u 个（全角 %u / 半角 %u / 空白 %u），最大 %dx%d\n",
                font.glyphCount, font.fullGlyphCount, font.halfGlyphCount,
                font.emptyGlyphCount, font.maxGlyphWidth, font.maxGlyphHeight);
    std::printf("  码点上限  : U+%04X\n", font.maxCodepoint);
    std::printf("  段大小    : presence %zu+%zu B | fullWidth %zu+%zu B | atlas %zu B | extras %u 个 (%zu B)\n",
                font.presenceBits.size() * 4u, font.presenceBlocks.size() * 4u,
                font.fullWidthBits.size() * 4u, font.fullWidthBlocks.size() * 4u,
                font.atlas.size() * 4u, font.extraCount, font.extraData.size() * 4u);
    std::printf("  体积      : v2 %.0f KB / v1 等价 %.0f KB（%+.1f%%），header 文本 %.2f MB\n",
                font.v2Bytes / 1024.0, font.v1Bytes / 1024.0,
                font.v1Bytes > 0 ? (static_cast<double>(font.v2Bytes) - static_cast<double>(font.v1Bytes)) * 100.0 / static_cast<double>(font.v1Bytes) : 0.0,
                font.headerBytes / (1024.0 * 1024.0));
}
