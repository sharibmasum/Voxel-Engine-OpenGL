//
// Created by sharib masum on 2025-06-15.
//

#include <cstdio>
#include <iostream>
#include "Window.h"


Window::Window() {
    width = 1280;
    height = 720;

    for (size_t i=0; i<1024; i++) {
        keys[i] = 0;
    }

    xChange = 0.0f;
    yChange = 0.0f;
    mouseFirstMoved = true;
}

Window::Window(GLuint windowWidth, GLuint windowHeight) {
    width = windowWidth;
    height = windowHeight;

    for (size_t i=0; i<1024; i++) {
        keys[i] = 0;
    }

    xChange = 0.0f;
    yChange = 0.0f;
    mouseFirstMoved = true;
}

int Window::initalize() {
    if (!glfwInit()) {
        printf("GLFW installation failed");
        glfwTerminate();
        return 1;

    }

    //window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // no backwards compatability
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // allows forward compatability

    mainWindow = glfwCreateWindow(width, height, "Learning OpenGL", NULL, NULL);
    if (!mainWindow) {
        printf("failed to create window");
        glfwTerminate();
        return 1;
    }

    // getting buffer size information
    glfwGetFramebufferSize(mainWindow, &bufferWidth, &bufferHeight); // setting buffer size information
    glfwMakeContextCurrent(mainWindow); // set context for glew to use
    createCallBacks(); // handles inputs
    glfwSetInputMode(mainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


    if (!gladLoadGL(glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // chunk meshes use consistent CCW winding; cull back faces for free
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glViewport(0,0, bufferWidth, bufferHeight);

    glfwSetWindowUserPointer(mainWindow, this);
    
    return 0;
}

GLfloat Window::getXChange() {
    GLfloat theChange = xChange;
    xChange = 0.0f;
    return theChange;
}

GLfloat Window::getYChange() {
    GLfloat theChange = yChange;
    yChange = 0.0f;
    return theChange;
}
void Window::handleKeys(GLFWwindow *window, int key, int code, int action, int mode) {
    Window* theWindow = static_cast<Window*>(glfwGetWindowUserPointer(window)); // how to get access to the window but dont know what this does speicfically

    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key >= 0 && key < 1024) {
        if(action == GLFW_PRESS) {
            theWindow -> keys[key] = true;
        } else if (action == GLFW_RELEASE){
            theWindow -> keys[key] = false;
        }
    }
 }

 void Window::handleMouse(GLFWwindow *window, double Xpos, double Ypos) {
     Window* theWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

     if (theWindow->mouseFirstMoved) {
         theWindow->lastX = Xpos; // assigning the current position to the pointer object
         theWindow->lastY = Ypos;
         theWindow->mouseFirstMoved = false;

     }

     theWindow->xChange = Xpos - theWindow->lastX;
     theWindow->yChange = theWindow->lastY - Ypos;
     theWindow->lastX = Xpos; // assigning the current position to the pointer object
     theWindow->lastY = Ypos;

}

 void Window::createCallBacks() {
     glfwSetKeyCallback(mainWindow, handleKeys);
     glfwSetCursorPosCallback(mainWindow, handleMouse);

 }

GLFWwindow* Window::getWindow() {
    return mainWindow;
}

Window::~Window() {
    glfwDestroyWindow(mainWindow);
    glfwTerminate();
}
