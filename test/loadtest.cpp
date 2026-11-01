#define NOMINMAX

#include <cstdio>
#include <string>
#include <vector>

// SDL_WindowIM.hpp 的实现部分必须在某一个翻译单元中定义此宏后包含
#define SDL_WINDOWIM_IMPLEMENTATION
#include "SDL_WindowIM.hpp"

#include "SDL_EventPlus.hpp"
#include "Times.hpp"
#include "DebugText.hpp"
#include "StopWatch.hpp"
#include "SmoothCpp.hpp"

// 三个字库 header 可以同时引用：每个 header 都会自动把自己注册进 dbtext 的字库表
#include "fonts/fusion_pixel_10px_mono_zh_hans.hpp"
#include "fonts/fusion_pixel_12px_mono_zh_hans.hpp"
#include "fonts/fusion_pixel_8px_mono_zh_hans.hpp"

// 由另一个翻译单元（test/odr_compat.cpp）提供，用于验证 ODR / 注册表共享
std::string dbtextOdrProbe();

int main(int, char**) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_WindowIM::WindowIM window;
    window.Create("多字库点阵字体测试", 1000, 800, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    window.SetDarkMode(true);
    window.SetCornerPreference(DWMWCP_ROUNDSMALL);

    SDL_Renderer* renderer = SDL_CreateRenderer(window.Get(), NULL);
    if (!renderer) {
        return -1;
    }

    const std::vector<std::string> fontIds = {
        "fusion-pixel-8px-mono-zh_hans",
        "fusion-pixel-10px-mono-zh_hans",
        "fusion-pixel-12px-mono-zh_hans",
    };

    size_t current = fontIds.size() - 1;
    dbtext::setFont(fontIds[current]);

    int scale = 4;

    SDL_Event eve;
    EventPlus evp(&eve);
    Timer fps_lim(60);
    StopWatch frameTimeCounter;
    EMASmoother fps_smoother(0.1f);
    bool mainloop = true;
    int c = 0;

    while (mainloop) {
        frameTimeCounter.Start();
        evp.reset();
        while (evp.poll()) {
            if (eve.type == SDL_EVENT_QUIT) {
                mainloop = false;
            } else if (eve.type == SDL_EVENT_KEY_DOWN) {
                if (eve.key.key == SDLK_ESCAPE) {
                    mainloop = false;
                } else if (eve.key.key == SDLK_1 || eve.key.key == SDLK_2 || eve.key.key == SDLK_3) {
                    const size_t index = static_cast<size_t>(eve.key.key - SDLK_1);
                    if (index < fontIds.size()) {
                        current = index;
                        dbtext::setFont(fontIds[current]);
                    }
                } else if (eve.key.key == SDLK_TAB) {
                    current = (current + 1) % fontIds.size();
                    dbtext::setFont(fontIds[current]);
                } else if (eve.key.key == SDLK_UP) {
                    if (scale < 10) ++scale;
                } else if (eve.key.key == SDLK_DOWN) {
                    if (scale > 1) --scale;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        const dbtext::FontDesc* font = dbtext::currentFont();

        paintDBTextFormat(renderer, 20, 20, { 0, 0, 0, 255 }, 2, { 0.0f, 0.0f },
                          "当前字库: %s (%ux%u)", font ? font->id : "无",
                          font ? font->cellWidth : 0u, font ? font->cellHeight : 0u);
        paintDBTextFormat(renderer, 20, 60, { 0, 0, 0, 255 }, 2, { 0.0f, 0.0f },
                          "1/2/3 或 Tab 切换字库，上下键调整缩放（当前 x%d）", scale);
        paintDBTextFormat(renderer, 20, 100, { 0, 0, 0, 255 }, 2, { 0.0f, 0.0f },
                          "已注册: %s", dbtextOdrProbe().c_str());

        const float fps = fps_smoother.process(1000.0f / frameTimeCounter.MillisecondPerf());
        paintDBTextFormat(renderer, 20, 140, { 0, 0, 0, 255 }, 2, { 0.0f, 0.0f }, "FPS:%.0f", fps);

        // 同一帧内按 FontID 分别绘制不同字库（8px 与 12px 演示背景色）
        dbtext::paintTextFormat(renderer, dbtext::FontID("fusion-pixel-8px-mono-zh_hans"),
                                20, 210, { 0, 0, 0, 255 }, { 214, 232, 255, 255 }, 6, { 0.0f, 0.0f },
                                "8px  中文点阵 Fusion Pixel! ABC abc 0123");
        const dbtext::FontDesc* font10 = dbtext::findFont("fusion-pixel-10px-mono-zh_hans");
        if (font10 != nullptr) {
            const SDL_Color color = { 0, 0, 0, 255 };
            dbtext::paintTextFormat(renderer, *font10, 20, 290, color, 5, { 0.0f, 0.0f },
                                    "10px 中文点阵 Fusion Pixel! ABC abc 0123（背景透明）");
        }
        dbtext::paintTextFormat(renderer, dbtext::FontID("fusion-pixel-12px-mono-zh_hans"),
                                20, 380, { 0, 0, 0, 255 }, { 255, 214, 128, 160 }, 4, { 0.0f, 0.0f },
                                "12px 中文点阵 Fusion Pixel! ABC abc 0123（半透明背景）");

        // 当前字库的大号示例（旧接口 paintDBTextFormat，跟随 setFont 的当前字库）
        paintDBTextFormat(renderer, 500, 640, { 0, 0, 0, 255 }, static_cast<float>(scale), { 0.5f, 0.5f },
                          "Hello,world!\n中文 Fusion Pixel!\n字体By:TakWolf\n%d:%c", c, static_cast<char>(c % 128));

        SDL_RenderPresent(renderer);
        fps_lim.Delay();
        frameTimeCounter.Stop();
        c++;
    }

    SDL_DestroyRenderer(renderer);
    window.Destroy();
    SDL_Quit();
    return 0;
}
