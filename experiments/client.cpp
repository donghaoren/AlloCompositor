#include <GLFW/glfw3.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <iostream>

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

    PFNGLXGETCONTEXTIDEXTPROC glXGetContextIDEXT = (PFNGLXGETCONTEXTIDEXTPROC)glXGetProcAddress((const GLubyte*)"glXGetContextIDEXT");

    GLXContext ctx = glXGetCurrentContext();
    GLXContextID ctxid = glXGetContextIDEXT(ctx);
    std::cout << "Context ID is: " << ctxid << std::endl;

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
