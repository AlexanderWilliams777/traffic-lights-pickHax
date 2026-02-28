#include "MyWindow.h"

MyWindow::MyWindow()
{
	width = 800;
	height = 600;
    xChange = 0.0f;
    yChange = 0.0f;

    mouseFirstMoved = true;

    for (size_t i = 0; i < 1024; i++)
    {
        keys[i] = 0;
    }
}

MyWindow::MyWindow(GLint windowWidth, GLint windowHeight)
{
	width = windowWidth;
	height = windowHeight;
    xChange = 0.0f;
    yChange = 0.0f;

    mouseFirstMoved = true;


    for (size_t i = 0; i < 1024; i++)
    {
        keys[i] = 0;
    }
}

int MyWindow::Initialise()
{
    //Initialize GLFW
    if (!glfwInit()) {
        printf("GLFW initialisation failed!");
        glfwTerminate();
        return 1;
    }

    // Setup GLFW window properties
    // OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // Core profile = No Backwards Compatability
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //Allow forward compatibility
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    mainWindow = glfwCreateWindow(width, height, "Test Window", NULL, NULL);
    if (!mainWindow) {
        printf("GLFW window creation failed!");
        glfwTerminate();
        return 1;
    }

    // Get Buffer size information
    glfwGetFramebufferSize(mainWindow, &bufferWidth, &bufferHeight);

    // Set context for GLEW to use
    glfwMakeContextCurrent(mainWindow);

    // Handle Key + Mouse Input
    createCallBacks();

    // Allow modern extension features
    glewExperimental = GL_TRUE;

    if (glewInit() != GLEW_OK) {
        printf("GLEW initialisation failed");
        glfwDestroyWindow(mainWindow);
        glfwTerminate();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);

    // Create viewport
    glViewport(0, 0, bufferWidth, bufferHeight);

    glfwSetWindowUserPointer(mainWindow, this);
}

void MyWindow::createCallBacks()
{
    glfwSetKeyCallback(mainWindow, handleKeys);
    glfwSetCursorPosCallback(mainWindow, handleMouse);
}

GLfloat MyWindow::getXChange()
{
    GLfloat theChange = xChange;
    xChange = 0.0f;
    return theChange;
}

GLfloat MyWindow::getYChange()
{
    GLfloat theChange = yChange;
    yChange = 0.0f;
    return theChange;
}

void MyWindow::handleKeys(GLFWwindow* window, int key, int code, int action, int mode)
{
    MyWindow* theWindow = static_cast<MyWindow*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS)
        {
            theWindow->keys[key] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            theWindow->keys[key] = false;
        }
    }
}

void MyWindow::handleMouse(GLFWwindow* window, double xPos, double yPos)
{
    MyWindow* theWindow = static_cast<MyWindow*>(glfwGetWindowUserPointer(window));

    if (theWindow->mouseFirstMoved)
    {
        theWindow->lastX = xPos;
        theWindow->lastY = yPos;
        theWindow->mouseFirstMoved = false;
    }

    theWindow->xChange = xPos - theWindow->lastX;
    theWindow->yChange = theWindow->lastY - yPos;

    theWindow->lastX = xPos;
    theWindow->lastY = yPos;
}

void MyWindow::checkAlt() {
    if (keys[GLFW_KEY_LEFT_ALT])
    {
        glfwSetInputMode(mainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    else
    {
        glfwSetInputMode(mainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

MyWindow::~MyWindow()
{
    glfwDestroyWindow(mainWindow);
    glfwTerminate();
}