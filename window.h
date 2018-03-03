#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Window {
    private:
        const int SCREEN_WIDTH   = 640;
        const int SCREEN_HEIGHT  = 480;
        const char* SCREEN_TITLE = "8080 Emulator";
        SDL_Window* window;
        SDL_Renderer* renderer;
    public:
        bool Initialize();
        void Render();
};

#endif
