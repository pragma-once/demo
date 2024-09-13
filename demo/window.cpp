#include "window.h"

#include <stdexcept>

namespace Demo
{
    int Window::object_count = 0;

    Window::Window(const std::string& title) : sdl_window(nullptr), gl_context(nullptr), should_close_flag(false)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);

        if (object_count == 0)
        {
            if (SDL_Init(SDL_INIT_VIDEO) != 0)
                throw std::runtime_error(SDL_GetError());
        }

        sdl_window = SDL_CreateWindow(
            title.data(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            800,
            600,
            SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP
        );
        if (sdl_window == nullptr)
        {
            if (object_count == 0)
            {
                SDL_Quit();
            }
            throw std::runtime_error(SDL_GetError());
        }

        gl_context = SDL_GL_CreateContext(sdl_window);
        if (gl_context == nullptr)
        {
            if (object_count == 0)
            {
                SDL_Quit();
            }
            throw std::runtime_error(SDL_GetError());
        }

        object_count++;

        make_current();
        gladLoadGL();
        SDL_GL_SetSwapInterval(1); // V-Sync ON
        glEnable(GL_MULTISAMPLE);

        int width, height;
        SDL_GetWindowSize(sdl_window, &width, &height);
        glViewport(0, 0, width, height);
    }

    Window::~Window()
    {
        close();
        object_count--;
        if (object_count == 0)
            SDL_Quit();
    }

    bool Window::is_open()
    {
        return sdl_window != nullptr && gl_context != nullptr;
    }

    void Window::make_current()
    {
        SDL_GL_MakeCurrent(sdl_window, gl_context);
    }

    void Window::update()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                should_close_flag = true;
            }
        }
    }

    void Window::swap_buffers()
    {
        SDL_GL_SwapWindow(sdl_window);
    }

    bool Window::should_close()
    {
        return should_close_flag;
    }

    void Window::close()
    {
        if (gl_context != nullptr)
        {
            SDL_GL_DeleteContext(gl_context);
            gl_context = nullptr;
        }
        if (sdl_window != nullptr)
        {
            SDL_DestroyWindow(sdl_window);
            sdl_window = nullptr;
        }
    }
}
