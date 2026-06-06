#define NOMINMAX

#include <cstdio>
#include <SDL_ttf.h>
#include "SDLWindowStuff.hpp"
#include "SDL_EventPlus.hpp"
#include "Times.hpp"
#include "DebugText.hpp"
#include "StopWatch.hpp"
#include "decode.hpp"
#include "SmoothCpp.hpp"
#include "GL_Commdlg.hpp"

int main(int,char**){
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    SDL_SetHint(SDL_HINT_IME_SHOW_UI,"1");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    SDLWindowStuff* sws;
    sws = sws->Create("生成字形文件",1000,800,SDL_WINDOW_ALLOW_HIGHDPI);
    sws->SetDarkMode(true);
    sws->SetPreference(3);
    SDL_Renderer* renderer = sws->CreateRenderer();

    TTF_Font* target_font = TTF_OpenFont("..\\font\\fusion-pixel-12px-monospaced-zh_hans.ttf",12);

    SDL_Event eve;
    EventPlus evp(&eve);
    bool mainloop = true;
    int c = 0;

    #define TASK_RANGE 65535

    FILE* file = fopen(".\\font.bin","wb");
    if(!file) {
        messageBox("任务失败","任务失败:无法打开文件",{{1,"好的"}},0,0);
        return -1;
    }

    int num_glyph = TASK_RANGE+1;

    fwrite(&num_glyph,sizeof(int),1,file);

    std::vector<int> glyph_map(num_glyph, 0);
    fwrite(glyph_map.data(),sizeof(int),glyph_map.size(),file);

    while(mainloop){
        evp.reset();
		while (evp.poll())
		{

			if(eve.type == SDL_QUIT){
				mainloop = false;
			}
		}

        SDL_Surface* glyph_raw = TTF_RenderGlyph32_Solid(target_font,c,{0,0,0,255});

        SDL_Surface* glyph = NULL; 

        if(glyph_raw){
            glyph = SDL_ConvertSurfaceFormat(glyph_raw,SDL_PIXELFORMAT_RGBA8888,0);
        }
        SDL_FreeSurface(glyph_raw);
        glyph_raw = NULL;

        glyph_map[c] = ftell(file);

        if(glyph){
            size_t image_size = (glyph->h * glyph->w + 7) / 8;
            std::vector<uint8_t> bit_compress(image_size, 0);
            for(int y = 0;y < glyph->h;y++){
                for(int x = 0;x < glyph->w;x++){
                    uint32_t& pix = *(uint32_t*)((uint8_t*)glyph->pixels + glyph->pitch * y + x * sizeof(uint32_t));

                    int pos = x + y * glyph->w;
                    
                    int byteIndex = pos / 8;
                    int bitIndex = 7 - (pos % 8);

                    if(pix == 0x000000FF){
                        bit_compress[byteIndex] |= (0 << bitIndex);
                    }else{
                        bit_compress[byteIndex] |= (1 << bitIndex);
                    }
                }
            }

            if(glyph->w > 255){
                messageBox("任务失败","字符宽度不应超过255",{{1,"好的"}},0,0);
                return -1;
            }

            else if(glyph->h > 255){
                messageBox("任务失败","字符高度不应超过255",{{1,"好的"}},0,0);
                return -1;
            }

            uint8_t bw = glyph->w;
            uint8_t bh = glyph->h;

            fwrite(&bw,sizeof(uint8_t),1,file);
            fwrite(&bh,sizeof(uint8_t),1,file);

            if(bw == 0 || bh == 0){
                messageBox("任务失败","?",{{1,"好的"}},0,0);
            }

            fwrite(bit_compress.data(),sizeof(uint8_t),bit_compress.size(),file);
        }else{
            uint8_t bw = 0;
            uint8_t bh = 0;

            fwrite(&bw,sizeof(uint8_t),1,file);
            fwrite(&bh,sizeof(uint8_t),1,file);
        }

        if((c % 100) == 0){

            if(!glyph){
                glyph = createDBTextSurface("NULL",{0,0,0,255});
            }

            SDL_SetRenderDrawColor(renderer,255,255,255,255);      
            SDL_RenderClear(renderer); 

            paintDBTextFormat(renderer,20,20,{0,0,0,255},2,{0,0},"正在生成字形文件...");

            paintDBTextFormat(renderer,500,500,{0,0,0,255},4,{0.5,0.5},"codepoint:%d",c);

            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer,glyph);

            SDL_Rect dst = {500,400,glyph->w*8,glyph->h*8};

            dst.x -= dst.w / 2;
            dst.y -= dst.h / 2;

            SDL_RenderCopy(renderer,tex,NULL,&dst);

            SDL_SetRenderDrawColor(renderer,255,0,0,255);
            SDL_RenderDrawRect(renderer,&dst);

            SDL_RenderPresent(renderer);

            SDL_DestroyTexture(tex);
        }

        if(glyph) SDL_FreeSurface(glyph);
        //SDL_Delay(10);
        if(c == TASK_RANGE) break;
        c++;
    }

    fseek(file,0,SEEK_SET);

    fwrite(&num_glyph,sizeof(int),1,file);

    fwrite(glyph_map.data(),sizeof(int),glyph_map.size(),file);

    fclose(file);

    SDL_DestroyRenderer(renderer);
    sws->Release();
    TTF_Quit();
    SDL_Quit();

    messageBox("任务完成","任务完成",{{1,"好的"}},0,0);

    return 0;
}