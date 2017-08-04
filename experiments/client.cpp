#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <zmq.h>

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
    glewInit();

    void* zmq_context = zmq_ctx_new();
    std::cout << "Request sending..." << std::endl;
    CUDASharedTextureClient client(zmq_context, "tcp://127.0.0.1:5930", 1024, 1024);

    std::cout << "Request sent" << std::endl;

    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, client.getOpenGLTexture(), 0);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        glClear(GL_COLOR_BUFFER_BIT);
        glBegin(GL_QUADS);
        glColor3f(0, 1, 0);
        glVertex3f(-0.5, -0.5, 0);
        glVertex3f(-0.5, +0.5, 0);
        glVertex3f(+0.5, +0.5, 0);
        glVertex3f(+0.5, -0.5, 0);
        glEnd();


        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        client.submit();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
