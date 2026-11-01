#ifndef DBTEXT_HEADER_EMITTER_HPP
#define DBTEXT_HEADER_EMITTER_HPP

/**
 * @file HeaderEmitter.hpp
 * @brief 把打包好的字库模型写成 C++ header（直接内嵌 constexpr 数组 + 自动注册）
 */

#include <string>

#include "FontBuilder.hpp"

/**
 * @brief 生成字库 header
 *
 * @param font       字库模型（会回填 headerBytes）
 * @param outputPath 输出文件路径（目录会自动创建）
 * @param error      失败原因
 * @param changed    true = 内容有变化并已写入；false = 内容相同，未改写文件
 * @return 是否成功
 */
bool emitFontHeader(PackedFont& font, const std::string& outputPath, std::string& error, bool& changed);

#endif // DBTEXT_HEADER_EMITTER_HPP
