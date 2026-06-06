#define NOMINMAX

#include <cstdio>
#include "SDLWindowStuff.hpp"
#include "SDL_EventPlus.hpp"
#include "Times.hpp"
#include "DebugText.hpp"
#include "StopWatch.hpp"
#include "SmoothCpp.hpp"

int main(int,char**){
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_SetHint(SDL_HINT_IME_SHOW_UI,"1");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    SDLWindowStuff* sws;
    sws = sws->Create("程序模板",1000,800,SDL_WINDOW_ALLOW_HIGHDPI);
    sws->SetDarkMode(true);
    sws->SetPreference(3);
    SDL_Renderer* renderer = sws->CreateRenderer();

    SDL_Event eve;
    EventPlus evp(&eve);
    Timer fps_lim(60);
    StopWatch frameTimeCounter;
    EMASmoother fps_smoother(0.1f);
    bool mainloop = true;
    int c = 0;
    while(mainloop){
        frameTimeCounter.Start();
        evp.reset();
		while (evp.poll())
		{

			if(eve.type == SDL_QUIT){
				mainloop = false;
			}
		}

        SDL_SetRenderDrawColor(renderer,255,255,255,255);      
        SDL_RenderClear(renderer); 

        int tg = (c / 30) % 128;
        paintDBTextFormat(renderer,500,400,{0,0,0,255},6,{0.5f,0.5f},"Hello,world!\n中文 Fusion Pixel!\n字体By:TakWolf\n%d:%c",tg,(char)tg);
        
        float fps = fps_smoother.process(1000.0f / frameTimeCounter.MillisecondPerf());
        paintDBTextFormat(renderer,10,10,{0,0,0,255},3,{0.0f,0.0f},"FPS:%.0f",fps);
        SDL_RenderPresent(renderer);
        fps_lim.Delay();
        frameTimeCounter.Stop();
        c++;
    }

    SDL_DestroyRenderer(renderer);
    sws->Release();
    SDL_Quit();
    return 0;
}