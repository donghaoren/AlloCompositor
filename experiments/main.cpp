#include <GLFW/glfw3.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <iostream>
#include <zmq.h>

#include <chrono>

#include "cuda_shared_memory.h"

int main(void)
{

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    void* zmq_context = zmq_ctx_new();
    CUDASharedTextureServer server(zmq_context, "tcp://127.0.0.1:5930");

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
            auto start = std::chrono::high_resolution_clock::now();

        server.mainThreadTick();

        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        for(int i = 0; i < 10; i++) {
            if(server.hasTexture(i)) {
                GLuint texture = server.getTexture(i);
                glBindTexture(GL_TEXTURE_2D, texture);
                glBegin(GL_QUADS);
                glColor3f(0, 1, 0);
                glTexCoord2f(0, 0);
                glVertex3f(-0.5, -0.5, 0);
                glTexCoord2f(0, 1);
                glVertex3f(-0.5, +0.5, 0);
                glTexCoord2f(1, 1);
                glVertex3f(+0.5, +0.5, 0);
                glTexCoord2f(1, 0);
                glVertex3f(+0.5, -0.5, 0);
                glEnd();
            }
        }

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        // operation to be timed ...

    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << 1.0e9 / (std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count()) << " fps\n";
    }

    glfwTerminate();
    return 0;
}
