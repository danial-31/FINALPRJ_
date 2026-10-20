#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[])
{
    const bool smokeTest = argc == 2 && std::string_view(argv[1]) == "--smoke-test";
    if (argc > 1 && !smokeTest) {
        std::cerr << "Usage: finalprj [--smoke-test]\n";
        return EXIT_FAILURE;
    }

    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "GLFW error " << code << ": " << description << '\n';
    });

    if (!glfwInit()) {
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    if (smokeTest) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "FINALPRJ | OpenGL", nullptr, nullptr);
    if (!window) {
        std::cerr << "Cannot create an OpenGL 3.3 Core window. Check your GPU driver.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    const int version = gladLoadGL(glfwGetProcAddress);
    if (!version || !GLAD_GL_VERSION_3_3) {
        std::cerr << "Cannot load OpenGL 3.3 functions.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION)
              << "\nRenderer: " << glGetString(GL_RENDERER) << '\n';
    glfwSwapInterval(smokeTest ? 0 : 1);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    int exitCode = EXIT_SUCCESS;
    int frames = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0) {
            glfwWaitEvents();
            continue;
        }

        // Use framebuffer pixels so resizing and high-DPI screens work correctly.
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Future scene rendering goes here.

        if (smokeTest) {
            unsigned char pixel[4] = {};
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            if (glGetError() != GL_NO_ERROR || pixel[0] || pixel[1] || pixel[2]) {
                std::cerr << "Smoke test failed: expected a black framebuffer without GL errors.\n";
                exitCode = EXIT_FAILURE;
                break;
            }
        }

        glfwSwapBuffers(window);
        if (smokeTest && ++frames == 3) {
            break;
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    if (smokeTest && exitCode == EXIT_SUCCESS) {
        if (frames != 3) {
            std::cerr << "Smoke test failed: window closed before three frames were rendered.\n";
            return EXIT_FAILURE;
        }
        std::cout << "Smoke test passed: rendered three black frames.\n";
    }
    return exitCode;
}
