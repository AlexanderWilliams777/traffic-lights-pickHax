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
#include "render/Shader.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "render/Mesh.h"

// Using Dear ImGui instead of MyGUI

// Window size
const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

// (stb_image available if you want to load textures for OpenGL)

// -------------------------
// Car struct
// -------------------------
struct Car {
    float x, y;
    float w, h;
    float speed;
    int dir; // 0=up,1=down,2=left,3=right
};

// Using Shader class in src/render/Shader.h

// Quad helper
#include "render/Quad.h"
#include <memory>
#include <limits>
#include "logging.h"
#include <csignal>

// Simple GL error checker
static void checkGLError(const char* when) {
    GLenum err;
    bool any = false;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "GL Error (" << when << "): 0x" << std::hex << err << std::dec << std::endl;
        any = true;
    }
    if (!any) {
        // optional: log that no error occurred
    }
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
    // initialize logging first (log file in runtime bin/logs/app.log)
    init_logging("logs/app.log");
    std::cout << "Application start" << std::endl;

    // Install simple crash/signal handlers so we capture crashes in the log
    // Use C signal APIs (global namespace)
    auto crashHandler = [](int sig)->void {
        std::cerr << "Fatal signal: " << sig << " - terminating" << std::endl;
        shutdown_logging();
        // restore default handler and re-raise to let OS handle it as well
        ::signal(sig, SIG_DFL);
        ::raise(sig);
    };
    ::signal(SIGSEGV, crashHandler);
    ::signal(SIGABRT, crashHandler);
    ::signal(SIGFPE, crashHandler);
    ::signal(SIGILL, crashHandler);
    std::set_terminate([](){
        std::cerr << "terminate() called - uncaught exception" << std::endl;
        shutdown_logging();
        std::abort();
    });

    // GLFW error callback
    glfwSetErrorCallback([](int error, const char* description){
        std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
    });

    if (!glfwInit()) {
        std::cerr << "glfwInit() failed" << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return -1;
    }

    std::cout << "glfwInit() succeeded" << std::endl;

    // window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Smart City", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create window\n"; std::cerr << "Press Enter to exit...\n"; std::cin.get(); return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        std::cerr << "Press Enter to exit...\n";
        std::cin.get();
        return -1;
    }
    std::cout << "GLAD initialized. OpenGL " << (const char*)glGetString(GL_VERSION) << std::endl;

    // setup optional GL debug callback (if available)
#if defined(GL_VERSION_4_3) || defined(GL_KHR_debug)
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(
        [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam){
            (void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam;
            std::cerr << "GL DEBUG: " << message << std::endl;
        },
        nullptr);
#endif
    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0.0f, 0.0f, 0.2f, 1.0f); // navy

    // initialize logging first (log file in runtime bin/logs/app.log)
    // note: init_logging already called above; avoid double-init
    std::cout << "Initializing subsystems..." << std::endl;

    // load shaders
    Shader quadShader;
    if (!quadShader.load("shaders/quad.vert.glsl", "shaders/quad.frag.glsl")) {
        std::cerr << "Failed to load quad shaders\n";
        std::cerr << "Make sure 'shaders' folder is next to the executable (bin/shaders).\n";
        std::cerr << "Press Enter to exit...\n";
        std::cin.get();
        return -1;
    }
    std::cout << "Quad shader loaded" << std::endl;

    // Note: skipping 3D model shader and camera setup — using 2D rendering only
    // ------------------------- Dear ImGui -------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    std::cout << "ImGui initialized" << std::endl;

    // ------------------------- Roads + Cars -------------------------

    Quad verticalRoad(300, 0, 50, HEIGHT);
    Quad horizontalRoad(0, 250, WIDTH, 50);

    std::vector<Car> cars = {
        {310,0,20,10,100.0f,0},
        {0,260,20,10,120.0f,3}
    };
    // 2D rectangles used to represent cars
    std::vector<std::unique_ptr<Quad>> carRects;
    carRects.push_back(std::make_unique<Quad>(cars[0].x, cars[0].y, cars[0].w, cars[0].h));
    carRects.push_back(std::make_unique<Quad>(cars[1].x, cars[1].y, cars[1].w, cars[1].h));

    // Model drawing flag (can be toggled at runtime via ImGui)
    bool drawModels = true;
    if (drawModels) std::cout << "Model drawing is ENABLED" << std::endl;

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
    std::cout << "Entering main loop" << std::endl;
    int frameCounter = 0;
    // log window-close events
    glfwSetWindowCloseCallback(window, [](GLFWwindow* wnd){
        (void)wnd;
        std::cout << "Window close requested" << std::endl;
    });

    try {
        while (!glfwWindowShouldClose(window)) {
            std::cout << "Frame " << frameCounter << " start" << std::endl;
            // Clear both color and depth so 3D rendering has a fresh depth buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            checkGLError("glClear");

        // (3D model drawing removed) — render 2D overlay below

        // Draw roads (2D quads) as overlay (depth test disabled)
        quadShader.use();
        quadShader.setMat4("uProjection", glm::ortho(0.0f, float(WIDTH), 0.0f, float(HEIGHT)));
        verticalRoad.draw(quadShader, glm::vec3(0.2f, 0.2f, 0.2f));
        horizontalRoad.draw(quadShader, glm::vec3(0.2f, 0.2f, 0.2f));

        std::cout << "After clearing frame" << std::endl;
        checkGLError("after clear");
        // Update + draw cars (frame-rate independent)
        double now = glfwGetTime();
        static double lastTime = now;
        float dt = float(now - lastTime);
        lastTime = now;

        if (!paused) updateCars(cars, dt);

        // Update 2D car rect positions and draw them using the quad shader
        for (size_t i = 0; i < cars.size(); ++i) {
            if (i < carRects.size()) {
                carRects[i]->update(cars[i].x, cars[i].y, cars[i].w, cars[i].h);
            }
        }
        quadShader.use();
        quadShader.setMat4("uProjection", glm::ortho(0.0f, float(WIDTH), 0.0f, float(HEIGHT)));
        for (auto &r : carRects) {
            r->draw(quadShader, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        // ---- ImGui frame ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (frameCounter == 0) std::cout << "First frame beginning" << std::endl;
        frameCounter++;

        ImGui::Begin("Controls");
        ImGui::Text("Cars: %d", (int)cars.size());
        ImGui::InputInt("Number of cars", &imgui_numCars);
        ImGui::Checkbox("Draw Models", &drawModels);
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
                    carRects.push_back(std::make_unique<Quad>(c.x, c.y, c.w, c.h));
                }
            } else {
                // remove cars and delete GL buffers
                for (int i = (int)cars.size() - 1; i >= target; --i) {
                    carRects.pop_back();
                    cars.pop_back();
                }
            }
        }

        // update per-car speed to globalSpeed (optional)
        for (auto &c : cars) c.speed = globalSpeed;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        checkGLError("ImGui RenderDrawData");
        glfwSwapBuffers(window);
        checkGLError("glfwSwapBuffers");
        glfwPollEvents();
        std::cout << "Frame " << frameCounter-1 << " end" << std::endl;
        // loop end
        }
    } catch (const std::exception &ex) {
        std::cerr << "Unhandled exception in main loop: " << ex.what() << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
    } catch (...) {
        std::cerr << "Unknown crash in main loop" << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
    }

    // ImGui cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    std::cout << "Application exiting. Press Enter to close..." << std::endl;
    shutdown_logging();
    std::cin.get();
    return 0;
}