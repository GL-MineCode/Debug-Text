#include "Preview.hpp"

#include <algorithm>
#include <cstdio>

PreviewWindow::~PreviewWindow() {
    destroy();
}

bool PreviewWindow::create(const std::string& title, const std::string& uiFontPath, std::string& error) {
    if (!SDL_CreateWindowAndRenderer(title.c_str(), 760, 460, 0, &window_, &renderer_)) {
        error = std::string("无法创建预览窗口：") + SDL_GetError();
        destroy();
        return false;
    }

    SDL_SetRenderVSync(renderer_, 1);

    if (!uiFontPath.empty()) {
        uiFont_ = TTF_OpenFont(uiFontPath.c_str(), 20.0f);
        if (uiFont_ != nullptr) TTF_SetFontHinting(uiFont_, TTF_HINTING_NONE);
    }

    status_ = "正在生成...";
    return true;
}

void PreviewWindow::destroy() {
    if (glyphTexture_ != nullptr) { SDL_DestroyTexture(glyphTexture_); glyphTexture_ = nullptr; }
    if (uiFont_ != nullptr) { TTF_CloseFont(uiFont_); uiFont_ = nullptr; }
    if (renderer_ != nullptr) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_ != nullptr) { SDL_DestroyWindow(window_); window_ = nullptr; }
}

bool PreviewWindow::alive() {
    SDL_Event event;
    bool running = true;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) running = false;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
    }
    return running;
}

void PreviewWindow::setStatus(const std::string& text) {
    status_ = text;
}

void PreviewWindow::progressCallback(void* user, const ProgressInfo& info) {
    PreviewWindow* self = static_cast<PreviewWindow*>(user);
    if (self == nullptr) return;

    self->percent_ = info.percent;
    if (info.bits != nullptr && info.width > 0 && info.height > 0) {
        self->glyphWidth_ = info.width;
        self->glyphHeight_ = info.height;
        self->glyphBits_.assign(info.bits,
                                info.bits + (static_cast<size_t>(info.width) * info.height + 7u) / 8u);
        char label[64];
        std::snprintf(label, sizeof(label), "U+%04X", info.codepoint);
        self->glyphLabel_ = label;
    }

    if (!self->alive()) return;
    self->renderFrame();
}

void PreviewWindow::renderFrame() {
    if (renderer_ == nullptr) return;

    SDL_SetRenderDrawColor(renderer_, 32, 34, 38, 255);
    SDL_RenderClear(renderer_);

    // 进度条
    SDL_FRect track{ 40.0f, 380.0f, 680.0f, 24.0f };
    SDL_SetRenderDrawColor(renderer_, 60, 64, 70, 255);
    SDL_RenderFillRect(renderer_, &track);

    SDL_FRect fill = track;
    fill.w = track.w * std::clamp(percent_, 0, 100) / 100.0f;
    SDL_SetRenderDrawColor(renderer_, 90, 170, 255, 255);
    SDL_RenderFillRect(renderer_, &fill);

    // 界面文字
    if (uiFont_ != nullptr) {
        char line[256];
        std::snprintf(line, sizeof(line), "%s  %d%%", status_.c_str(), percent_);
        SDL_Surface* surface = TTF_RenderText_Solid(uiFont_, line, 0, SDL_Color{ 230, 232, 236, 255 });
        if (surface != nullptr) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture != nullptr) {
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
                SDL_FRect dst{ 40.0f, 420.0f, static_cast<float>(surface->w), static_cast<float>(surface->h) };
                SDL_RenderTexture(renderer_, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_DestroySurface(surface);
        }

        if (!glyphLabel_.empty()) {
            SDL_Surface* glyphSurface = TTF_RenderText_Solid(uiFont_, glyphLabel_.c_str(), 0, SDL_Color{ 200, 200, 200, 255 });
            if (glyphSurface != nullptr) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, glyphSurface);
                if (texture != nullptr) {
                    SDL_FRect dst{ 560.0f, 40.0f, static_cast<float>(glyphSurface->w), static_cast<float>(glyphSurface->h) };
                    SDL_RenderTexture(renderer_, texture, nullptr, &dst);
                    SDL_DestroyTexture(texture);
                }
                SDL_DestroySurface(glyphSurface);
            }
        }
    }

    // 当前字形（放大显示）
    if (!glyphBits_.empty() && glyphWidth_ > 0 && glyphHeight_ > 0) {
        if (glyphTexture_ == nullptr || glyphTextureWidth_ != glyphWidth_ || glyphTextureHeight_ != glyphHeight_) {
            if (glyphTexture_ != nullptr) SDL_DestroyTexture(glyphTexture_);
            glyphTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                              SDL_TEXTUREACCESS_STREAMING, glyphWidth_, glyphHeight_);
            glyphTextureWidth_ = glyphWidth_;
            glyphTextureHeight_ = glyphHeight_;
        }
        if (glyphTexture_ != nullptr) {
            std::vector<uint32_t> pixels(static_cast<size_t>(glyphWidth_) * glyphHeight_, 0u);
            for (int y = 0; y < glyphHeight_; ++y) {
                for (int x = 0; x < glyphWidth_; ++x) {
                    const int index = y * glyphWidth_ + x;
                    const bool ink = ((glyphBits_[index >> 3] >> (7 - (index & 7))) & 1) != 0;
                    pixels[index] = ink ? 0xFFFFFFFFu : 0x00000000u;
                }
            }

            SDL_UpdateTexture(glyphTexture_, nullptr, pixels.data(), glyphWidth_ * static_cast<int>(sizeof(uint32_t)));
            SDL_SetTextureScaleMode(glyphTexture_, SDL_SCALEMODE_NEAREST);

            const float scale = std::min(320.0f / static_cast<float>(glyphWidth_),
                                         320.0f / static_cast<float>(glyphHeight_));
            SDL_FRect dst{ 40.0f, 40.0f, glyphWidth_ * scale, glyphHeight_ * scale };
            SDL_RenderTexture(renderer_, glyphTexture_, nullptr, &dst);
            SDL_SetRenderDrawColor(renderer_, 90, 170, 255, 255);
            SDL_RenderRect(renderer_, &dst);
        }
    }

    SDL_RenderPresent(renderer_);
}
