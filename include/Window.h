//
// Created by sharib masum on 2025-06-15.
//

#include <stdio.h>

#include "gl.h"
#include <GLFW/glfw3.h>

#ifndef BASICOPENGL_WINDOW_H
#define BASICOPENGL_WINDOW_H


class Window {


public:
    Window();
    Window(GLuint windowWidth, GLuint windowHeight);

    int initalize();

    GLfloat getBufferWidth() {
        return bufferWidth;
    }

    GLfloat getBufferHeight() {
        return bufferHeight;
    }

    bool getShouldClose() {
        return glfwWindowShouldClose(mainWindow);
    }

    bool* getsKeys() {
        return keys;
    }

    GLfloat getXChange();
    GLfloat getYChange();

    // Call after switching GLFW_CURSOR mode so the next mouse delta doesn't
    // include the jump caused by the OS warping/releasing the cursor.
    void resetMouseDelta() {
        mouseFirstMoved = true;
        xChange = 0.0f;
        yChange = 0.0f;
    }


    void swapBuffers() {
        glfwSwapBuffers(mainWindow);
    }

    GLFWwindow* getWindow();

    ~Window();

private:
    GLFWwindow *mainWindow;

    GLint width, height;
    GLint bufferWidth, bufferHeight;

    bool keys[1024];

    static void handleKeys(GLFWwindow *window, int key, int code, int action, int mode);
    static void handleMouse (GLFWwindow *window,  double Xpos, double Ypos);

    void createCallBacks();

    GLfloat lastX, lastY, xChange, yChange;
    bool mouseFirstMoved;


};


#endif //BASICOPENGL_WINDOW_H
