#pragma once

#include "demo.dec.h"

#include "../glad/include/glad/glad.h"
#include "../sdl2/include/SDL.h"

#include <string>

namespace Demo
{
    class Window final
    {
    public:
        Window(const std::string& title = "Demo");
        ~Window();

        Window(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(const Window&) = delete;
        Window& operator=(Window&&) = delete;

        bool is_open();
        void make_current();
        void update();
        void swap_buffers();
        bool should_close();
        void close();
    private:
        static int object_count;

        SDL_Window * sdl_window;
        SDL_GLContext gl_context;
        bool should_close_flag;
    };
}
