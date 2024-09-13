#include "demo.h"

int main()
{
    Demo::Window window;

    while (window.is_open())
    {
        window.update();
        if (window.should_close())
            window.close();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        window.swap_buffers();
    }

    return 0;
}
