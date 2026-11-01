/**
 * @file odr_compat.cpp
 * @brief 第二个翻译单元：验证 DebugText.hpp + 多个字库 header 同时被多个 TU 包含时不会出现
 *        重复定义（ODR）问题，同时也验证字库自动注册在跨 TU 时共享同一个注册表。
 */

#include <string>

#include "DebugText.hpp"

#include "fonts/fusion_pixel_10px_mono_zh_hans.hpp"
#include "fonts/fusion_pixel_12px_mono_zh_hans.hpp"
#include "fonts/fusion_pixel_8px_mono_zh_hans.hpp"

std::string dbtextOdrProbe() {
    std::string result;
    for (dbtext::FontID id : dbtext::fontIds()) {
        const dbtext::FontDesc* font = dbtext::findFont(id);
        result += std::string(id);
        result += '(';
        result += std::to_string(font != nullptr ? font->glyphCount : 0u);
        result += ") ";
    }
    return result;
}
