#pragma once

#include <stdio.h>

#include <GL\glew.h>
#include <GLFW\glfw3.h>

class MyWindow
{
public:
	MyWindow();
	MyWindow(GLint windowWidth, GLint windowHeight);

	int Initialise();

	GLfloat getBufferWidth() { return bufferWidth; }
	GLfloat getBufferHeight() { return bufferHeight; }

	bool getShouldClose() { return glfwWindowShouldClose(mainWindow); }

	bool* getKeys() { return keys; } // Bad design to return whole array instead of checking and passing individual key, for tutorial purpposes
	void checkAlt();
	GLFWwindow* getMainWindow() const { return mainWindow; }
	GLfloat getXChange();
	GLfloat getYChange();

	void swapBuffers() { glfwSwapBuffers(mainWindow); }

	~MyWindow();

private:
	GLFWwindow* mainWindow;

	GLint width, height;
	GLint bufferWidth, bufferHeight;

	bool keys[1024];

	// Should add a bool to check if the mouse has moved since the last time we got the xChange and yChange instead of resetting the mouse to 0 each time

	GLfloat lastX, lastY, xChange, yChange;
	bool mouseFirstMoved;
	bool mouseMovedSinceLast;

	void createCallBacks();
	static void handleKeys(GLFWwindow* window, int key, int code, int action, int mode);
	static void handleMouse(GLFWwindow* window, double xPos, double yPos);
};

