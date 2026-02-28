#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <vector>
#include <iostream>
#include <cstring>
// Provide stb_image implementation in this translation unit
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Using Dear ImGui instead of MyGUI

// Window size
const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

// (stb_image available if you want to load textures for OpenGL)

// -------------------------
// Shader sources
// -------------------------
const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
})";

const char* fragmentShaderSrc = R"(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
})";

// -------------------------
// Car struct
// -------------------------
struct Car {
    float x, y;
    float w, h;
    float speed;
    int dir; // 0=up,1=down,2=left,3=right
};

// -------------------------
// Compile shader
// -------------------------
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cout << "Shader compile error: " << info << std::endl;
    }
    return shader;
}

// -------------------------
// Shader program
// -------------------------
GLuint createShaderProgram() {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        std::cout << "Program link error: " << info << std::endl;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

// -------------------------
// Rectangle VAO/VBO
// -------------------------
struct Rect {
    GLuint VAO, VBO;
};

Rect createRect(float x, float y, float w, float h) {
    Rect r;
    float verts[] = {
        x, y, x + w, y, x + w, y + h,
        x, y, x + w, y + h, x, y + h
    };
    glGenVertexArrays(1, &r.VAO);
    glGenBuffers(1, &r.VBO);
    glBindVertexArray(r.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, r.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return r;
}

void updateRect(Rect& r, float x, float y, float w, float h) {
    float verts[] = {
        x, y, x + w, y, x + w, y + h,
        x, y, x + w, y + h, x, y + h
    };
    glBindBuffer(GL_ARRAY_BUFFER, r.VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawRect(const Rect& r, GLuint program, float rC, float gC, float bC) {
    glUseProgram(program);
    GLuint loc = glGetUniformLocation(program, "uColor");
    glUniform3f(loc, rC, gC, bC);
    glBindVertexArray(r.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// -------------------------
// Update car positions
// -------------------------
void updateCars(std::vector<Car>& cars, float dt) {
    for (auto& car : cars) {
        switch (car.dir) {
        case 0: car.y += car.speed * dt; break;
        case 1: car.y -= car.speed * dt; break;
        case 2: car.x -= car.speed * dt; break;
        case 3: car.x += car.speed * dt; break;
        }
        if (car.x > WIDTH) car.x = -car.w;
        if (car.x + car.w < 0) car.x = WIDTH;
        if (car.y > HEIGHT) car.y = -car.h;
        if (car.y + car.h < 0) car.y = HEIGHT;
    }
}

int main() {
    // ------------------------- Window + GLAD -------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Smart City", nullptr, nullptr);
    if (!window) { std::cout << "Failed to create window\n"; return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cout << "Failed to initialize GLAD\n"; return -1; }
    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0.0f, 0.0f, 0.2f, 1.0f); // navy

    GLuint shaderProgram = createShaderProgram();
    glm::mat4 projection = glm::ortho(0.0f, float(WIDTH), 0.0f, float(HEIGHT));
    GLuint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
    // ------------------------- Dear ImGui -------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // ------------------------- Roads + Cars -------------------------
    Rect verticalRoad = createRect(300, 0, 50, HEIGHT);
    Rect horizontalRoad = createRect(0, 250, WIDTH, 50);

    std::vector<Car> cars = {
        {310,0,20,10,100.0f,0},
        {0,260,20,10,120.0f,3}
    };
    std::vector<Rect> carRects = {
        createRect(cars[0].x,cars[0].y,cars[0].w,cars[0].h),
        createRect(cars[1].x,cars[1].y,cars[1].w,cars[1].h)
    };

    // ImGui-controlled parameters
    int imgui_numCars = (int)cars.size();
    float globalSpeed = 100.0f;
    bool paused = false;

    // HUD (MyGUI can be enabled later)
#ifdef USE_MYGUI
    // MyGUI label
    MyGUI::TextBox* label = gui->createWidget<MyGUI::TextBox>(
        "TextBox", 10, HEIGHT - 30, 200, 20,
        MyGUI::Align::Default, "Main", "CarCount"
    );
    label->setCaption("Cars moving: 0");
#else
    std::string hudText = "Cars moving: 0";
#endif

    // ------------------------- Main loop -------------------------
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw roads
        drawRect(verticalRoad, shaderProgram, 0.2f, 0.2f, 0.2f);
        drawRect(horizontalRoad, shaderProgram, 0.2f, 0.2f, 0.2f);

        // Update + draw cars (frame-rate independent)
        double now = glfwGetTime();
        static double lastTime = now;
        float dt = float(now - lastTime);
        lastTime = now;

        if (!paused) updateCars(cars, dt);
        for (int i = 0; i < cars.size(); ++i) {
            updateRect(carRects[i], cars[i].x, cars[i].y, cars[i].w, cars[i].h);
            drawRect(carRects[i], shaderProgram, 1.0f, 0.0f, 0.0f);
        }
        // ---- ImGui frame ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Controls");
        ImGui::Text("Cars: %d", (int)cars.size());
        ImGui::InputInt("Number of cars", &imgui_numCars);
        if (imgui_numCars < 0) imgui_numCars = 0;
        if (ImGui::Button(paused ? "Resume" : "Pause")) paused = !paused;
        ImGui::SameLine();
        if (ImGui::Button("Reset Positions")) {
            for (size_t i = 0; i < cars.size(); ++i) { cars[i].x = 0; cars[i].y = 0; }
        }
        ImGui::SliderFloat("Global speed", &globalSpeed, 10.0f, 500.0f);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        // apply ImGui-driven changes: resize cars
        if (imgui_numCars != (int)cars.size()) {
            int target = imgui_numCars;
            if (target > (int)cars.size()) {
                // add cars
                for (int i = cars.size(); i < target; ++i) {
                    Car c = {10.0f * i, 260.0f, 20.0f, 10.0f, globalSpeed, 3};
                    cars.push_back(c);
                    carRects.push_back(createRect(c.x, c.y, c.w, c.h));
                }
            } else {
                // remove cars and delete GL buffers
                for (int i = (int)cars.size() - 1; i >= target; --i) {
                    glDeleteBuffers(1, &carRects[i].VBO);
                    glDeleteVertexArrays(1, &carRects[i].VAO);
                    carRects.pop_back();
                    cars.pop_back();
                }
            }
        }

        // update per-car speed to globalSpeed (optional)
        for (auto &c : cars) c.speed = globalSpeed;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ImGui cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}