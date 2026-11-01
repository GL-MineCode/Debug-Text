#ifndef DBTEXT_FONT_BUILDER_HPP
#define DBTEXT_FONT_BUILDER_HPP

/**
 * @file FontBuilder.hpp
 * @brief 把 TrueType 字体栅格化成 DebugText v2 点阵字库模型
 */

#include <cstdint>
#include <string>
#include <vector>

//! 进度回调携带的信息
struct ProgressInfo {
    int percent = 0;             //!< 0 ~ 100
    uint32_t codepoint = 0;      //!< 当前处理的码点
    const uint8_t* bits = nullptr; //!< 当前字形位图（1 = 墨点，按行紧密排列；可为空）
    int width = 0;
    int height = 0;
};

using ProgressCallback = void (*)(void* user, const ProgressInfo& info);

struct FontBuildOptions {
    std::string ttfPath;                              //!< 源 TTF 路径
    std::string id;                                   //!< FontID（唯一）
    std::string displayName;                          //!< 为空则使用 TTF 的 family name
    std::string copyright;                            //!< 版权声明（写入生成的 header）
    std::string licenseName = "SIL Open Font License 1.1";
    std::string sourceVersion;                        //!< 源字体版本（写入 header）
    int ptsize = 0;                                   //!< 点大小，0 = 自动
    int cellWidth = 0;                                //!< 全角宽，0 = ptsize
    int cellHeight = 0;                               //!< 单元格高，0 = ptsize
    int tabColumns = 4;                               //!< Tab = tabColumns 个全角宽
    uint32_t maxCodepoint = 0x10FFFFu;                //!< 扫描上界（含）
    bool verbose = true;
    ProgressCallback onProgress = nullptr;
    void* progressUser = nullptr;
};

//! 打包好的 v2 字库模型（与 DebugText.hpp 的 FontDesc 字段一一对应）
struct PackedFont {
    // 元信息
    std::string id;
    std::string displayName;
    std::string styleName;
    std::string copyright;
    std::string licenseName;
    std::string sourceFile;
    std::string sourceVersion;

    // 度量
    uint32_t cellWidth = 0;
    uint32_t cellHeight = 0;
    uint32_t halfWidth = 0;
    uint32_t tabWidth = 0;

    // 索引
    uint32_t maxCodepoint = 0;
    uint32_t glyphCount = 0;
    std::vector<uint32_t> presenceBits;          // 已补齐到整块（256 码点 = 8 词）
    std::vector<uint32_t> presenceBlocks;
    std::vector<uint32_t> fullWidthBits;         // 已补齐到整块（256 字形 = 8 词）
    std::vector<uint32_t> fullWidthBlocks;

    // 负载
    uint32_t halfRecordBytes = 0;
    uint32_t fullRecordBytes = 0;
    std::vector<uint32_t> atlas;                 // 位图字节流（按 4 字节打包成词）

    // 超格字形
    uint32_t extraCount = 0;
    std::vector<uint32_t> extraBits;
    std::vector<uint32_t> extraBlocks;
    std::vector<uint16_t> extraDims;             // w | (h << 8)
    std::vector<uint32_t> extraOffsets;
    std::vector<uint32_t> extraData;

    // 统计
    uint64_t v1Bytes = 0;                        //!< 等价的 v1 格式体积
    uint64_t v2Bytes = 0;                        //!< v2 各段体积之和
    int maxGlyphWidth = 0;
    int maxGlyphHeight = 0;
    uint32_t emptyGlyphCount = 0;
    uint32_t halfGlyphCount = 0;
    uint32_t fullGlyphCount = 0;
    size_t headerBytes = 0;                      //!< 生成的 header 文本大小（由 emitter 回填）
};

/**
 * @brief 把 TTF 栅格化并打包成 v2 字库模型
 * @param options 生成选项
 * @param out     输出模型
 * @param error   失败原因
 * @return 是否成功
 */
bool buildFontFromTTF(const FontBuildOptions& options, PackedFont& out, std::string& error);

/**
 * @brief 打印体积报告
 */
void printFontReport(const PackedFont& font);

#endif // DBTEXT_FONT_BUILDER_HPP
