#ifndef DBTEXT_PREVIEW_HPP
#define DBTEXT_PREVIEW_HPP

/**
 * @file Preview.hpp
 * @brief generator 的可选预览窗口（--preview）
 */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdint>
#include <string>
#include <vector>

#include "FontBuilder.hpp"

class PreviewWindow {
public:
    ~PreviewWindow();

    /**
     * @brief 创建预览窗口
     * @param title    窗口标题
     * @param uiFont   用于绘制界面文字所用的 TTF 路径（源字体即可）
     */
    bool create(const std::string& title, const std::string& uiFont, std::string& error);
    void destroy();

    //! 窗口是否仍然存活（用户点击关闭后返回 false）
    bool alive();

    void setStatus(const std::string& text);

    //! 供 FontBuilder 使用的进度回调
    static void progressCallback(void* user, const ProgressInfo& info);

private:
    void renderFrame();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* uiFont_ = nullptr;
    SDL_Texture* glyphTexture_ = nullptr;
    int glyphTextureWidth_ = 0;
    int glyphTextureHeight_ = 0;

    std::string status_;
    std::string glyphLabel_;
    int percent_ = 0;
    std::vector<uint8_t> glyphBits_;
    int glyphWidth_ = 0;
    int glyphHeight_ = 0;
};

#endif // DBTEXT_PREVIEW_HPP
