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
#include "simulate/Simulation.h"

// Using Dear ImGui instead of MyGUI

// Window size
const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

// (stb_image available if you want to load textures for OpenGL)

// Simulation provides all vehicle state and positions; no local 2D car struct needed.

// Using Shader class in src/render/Shader.h

// Quad helper
#include "render/Quad.h"
#include <memory>
#include <limits>
#include "logging.h"
#include <csignal>
#include <cmath>

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

// Helper: draw a rotated rectangle by uploading two triangles (in world coords)
static void drawRotatedRect(const Shader &shader, float cx, float cy, float w, float h, float angleRad, const glm::vec3 &color) {
    // compute corners
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    float c = cosf(angleRad);
    float s = sinf(angleRad);
    float corners[8];
    // p0 = (-hw, -hh)
    corners[0] = cx + (-hw)*c - (-hh)*s;
    corners[1] = cy + (-hw)*s + (-hh)*c;
    // p1 = ( hw, -hh)
    corners[2] = cx + (hw)*c - (-hh)*s;
    corners[3] = cy + (hw)*s + (-hh)*c;
    // p2 = ( hw,  hh)
    corners[4] = cx + (hw)*c - (hh)*s;
    corners[5] = cy + (hw)*s + (hh)*c;
    // p3 = (-hw, hh)
    corners[6] = cx + (-hw)*c - (hh)*s;
    corners[7] = cy + (-hw)*s + (hh)*c;

    float verts[12] = {
        corners[0], corners[1],
        corners[2], corners[3],
        corners[4], corners[5],
        corners[0], corners[1],
        corners[4], corners[5],
        corners[6], corners[7]
    };

    GLuint VAO = 0, VBO = 0;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    shader.use();
    shader.setVec3("uColor", color);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}

// Draw an arrow: shaft (rect) and head (small rect) at given center, angle, length
static void drawArrow(const Shader &shader, float cx, float cy, float length, float angleRad, const glm::vec3 &color) {
    // shaft centered at half-length
    float shaftLen = length * 0.6f;
    float shaftW = 3.0f;
    float sx = cx + cosf(angleRad) * (shaftLen * 0.5f);
    float sy = cy + sinf(angleRad) * (shaftLen * 0.5f);
    drawRotatedRect(const_cast<Shader&>(shader), sx, sy, shaftLen, shaftW, angleRad, color);
    // head
    float headLen = length * 0.25f;
    float hx = cx + cosf(angleRad) * (shaftLen * 0.5f + headLen * 0.5f);
    float hy = cy + sinf(angleRad) * (shaftLen * 0.5f + headLen * 0.5f);
    drawRotatedRect(const_cast<Shader&>(shader), hx, hy, headLen, shaftW * 1.4f, angleRad, color);
}

// No local per-frame car integration here; Simulation::update() advances vehicle state.

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
    // Simulation core
    Simulation sim(0.016f); // fixed simulation timestep
    sim.initializeIntersection();
    // default strategy: Density (index 1)
    sim.setControllerStrategyByIndex(1);
    // Populate simulation with many random cars for a busier scenario
    sim.createCars(200, -1);
    // Run one update to allow spawn logic to populate initial cars
    sim.update(0.016f);
    float viewScale = 4.0f; // map simulation coords to screen
    float xOffset = 200.0f;
    float yOffset = 100.0f;

    // Camera pointer struct for scroll callback
    struct CameraPtr { float* viewScale; float* xOffset; float* yOffset; };
    CameraPtr *camPtr = new CameraPtr{ &viewScale, &xOffset, &yOffset };
    glfwSetWindowUserPointer(window, camPtr);
    // scroll to zoom (zoom around cursor)
    glfwSetScrollCallback(window, [](GLFWwindow* w, double /*xoff*/, double yoff){
        auto p = static_cast<CameraPtr*>(glfwGetWindowUserPointer(w));
        if (!p) return;
        float &vs = *p->viewScale;
        float &xo = *p->xOffset;
        float &yo = *p->yOffset;
        double mx, my; glfwGetCursorPos(w, &mx, &my);
        // world coords before zoom
        float wx = (static_cast<float>(mx) - xo) / vs;
        float wy = (static_cast<float>(HEIGHT - my) - yo) / vs;
        // scale factor
        float factor = std::pow(1.12f, (float)yoff);
        vs *= factor;
        if (vs < 0.2f) vs = 0.2f;
        if (vs > 40.0f) vs = 40.0f;
        // adjust offsets so cursor stays over same world point
        xo = static_cast<float>(mx) - wx * vs;
        yo = static_cast<float>(HEIGHT - my) - wy * vs;
    });

    // visualization toggles
    bool showLights = true;
    bool showNodes = true;
    bool showStopLines = true;
    bool showHeatmap = false;
    int selectedEdgeIdx = -1;

    // Model drawing flag (can be toggled at runtime via ImGui)
    bool drawModels = true;
    if (drawModels) std::cout << "Model drawing is ENABLED" << std::endl;

    // ImGui-controlled parameters
    float globalSpeed = 100.0f;
    bool paused = false;
    int imgui_create_count = 1;

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
        // per-frame verbose logging disabled to avoid flooding output
            // Clear both color and depth so 3D rendering has a fresh depth buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            checkGLError("glClear");

        // (3D model drawing removed) — render 2D overlay below

        // Draw roads (based on simulation network)
        quadShader.use();
        quadShader.setMat4("uProjection", glm::ortho(0.0f, float(WIDTH), 0.0f, float(HEIGHT)));
        const auto &network = sim.getNetwork();
        const float baseRoadWidth = 6.0f; // base width that scales with viewScale
        float roadWidth = baseRoadWidth * viewScale;
        for (const auto &e : network.edges) {
            if (!e.start || !e.end) continue;
            float sx = e.start->position.x * viewScale + xOffset;
            float sy = e.start->position.y * viewScale + yOffset;
            float ex = e.end->position.x * viewScale + xOffset;
            float ey = e.end->position.y * viewScale + yOffset;
            float dx = ex - sx;
            float dy = ey - sy;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len <= 0.0f) continue;
            float cx = (sx + ex) * 0.5f;
            float cy = (sy + ey) * 0.5f;
            float angle = atan2f(dy, dx);
            drawRotatedRect(quadShader, cx, cy, len, roadWidth, angle, glm::vec3(0.2f, 0.2f, 0.2f));
        }

        // draw nodes
        if (showNodes) {
            for (const auto &n : network.nodes) {
                float nx = n.position.x * viewScale + xOffset;
                float ny = n.position.y * viewScale + yOffset;
                drawRotatedRect(quadShader, nx, ny, 8.0f, 8.0f, 0.0f, glm::vec3(0.15f, 0.15f, 0.15f));
            }
        }

        // draw traffic lights
        if (showLights) {
            const auto &ctrl = sim.getController();
            for (const auto &l : ctrl.lights) {
                if (!l.controlledEdge || !l.controlledEdge->start || !l.controlledEdge->end) continue;
                // place the light slightly before the end of the edge
                float sx = l.controlledEdge->start->position.x;
                float sy = l.controlledEdge->start->position.y;
                float ex = l.controlledEdge->end->position.x;
                float ey = l.controlledEdge->end->position.y;
                float dx = ex - sx;
                float dy = ey - sy;
                float len = std::sqrt(dx*dx + dy*dy);
                float offset = std::max(0.05f, 0.08f * len);
                float t = (len > 0.0f) ? ((len - offset) / len) : 0.0f;
                float px = (sx + (ex - sx) * t) * viewScale + xOffset;
                float py = (sy + (ey - sy) * t) * viewScale + yOffset;
                glm::vec3 color(1.0f, 0.0f, 0.0f);
                switch (l.state) {
                case LightState::Red: color = glm::vec3(1.0f, 0.0f, 0.0f); break;
                case LightState::Straight: color = glm::vec3(0.0f, 1.0f, 0.0f); break;
                case LightState::Left: color = glm::vec3(1.0f, 1.0f, 0.0f); break;
                case LightState::Right: color = glm::vec3(0.0f, 1.0f, 1.0f); break;
                }
                drawRotatedRect(quadShader, px, py, 10.0f, 10.0f, 0.0f, color);
                // draw allowed direction arrows
                float ex2 = l.controlledEdge->end->position.x;
                float ey2 = l.controlledEdge->end->position.y;
                float sx2 = l.controlledEdge->start->position.x;
                float sy2 = l.controlledEdge->start->position.y;
                float dx2 = ex2 - sx2;
                float dy2 = ey2 - sy2;
                float baseAngle = atan2f(dy2, dx2);
                float arrowLen = 20.0f;
                // straight
                if (l.allowStraight) {
                    drawArrow(quadShader, px, py, arrowLen, baseAngle, glm::vec3(0.0f, 1.0f, 0.0f));
                }
                // left (angle + 90deg)
                if (l.allowLeft) {
                    drawArrow(quadShader, px, py, arrowLen, baseAngle + 1.57079633f, glm::vec3(1.0f, 1.0f, 0.0f));
                }
                // right (angle - 90deg)
                if (l.allowRight) {
                    drawArrow(quadShader, px, py, arrowLen, baseAngle - 1.57079633f, glm::vec3(0.0f, 1.0f, 1.0f));
                }
            }
        }

        // preview spawn location for selected edge
        if (selectedEdgeIdx >= 0 && selectedEdgeIdx < static_cast<int>(network.edges.size())) {
            const auto &pe = network.edges[selectedEdgeIdx];
            if (pe.start && pe.end) {
                float sx = pe.start->position.x * viewScale + xOffset;
                float sy = pe.start->position.y * viewScale + yOffset;
                float ex = pe.end->position.x * viewScale + xOffset;
                float ey = pe.end->position.y * viewScale + yOffset;
                float dx = ex - sx;
                float dy = ey - sy;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len > 0.0f) {
                    float t = 0.05f; // small offset from start
                    float px = sx + dx * t;
                    float py = sy + dy * t;
                    // draw preview as small green rectangle
                    drawRotatedRect(quadShader, px, py, 8.0f, 6.0f, atan2f(dy, dx), glm::vec3(0.0f, 1.0f, 0.0f));
                }
            }
        }

        checkGLError("after clear");
        // Update + draw cars (frame-rate independent)
        double now = glfwGetTime();
        static double lastTime = now;
        float dt = float(now - lastTime);
        lastTime = now;

        if (!paused) sim.update(dt);

        // Draw cars from simulation
        const auto &simCars = sim.getCars();
        for (const auto &c : simCars) {
            if (!c.currentEdge || !c.currentEdge->start || !c.currentEdge->end) continue;
            float t = (c.currentEdge->length > 0.0f) ? (c.s / c.currentEdge->length) : 0.0f;
            float sx = c.currentEdge->start->position.x * viewScale + xOffset;
            float sy = c.currentEdge->start->position.y * viewScale + yOffset;
            float ex = c.currentEdge->end->position.x * viewScale + xOffset;
            float ey = c.currentEdge->end->position.y * viewScale + yOffset;
            float px = (sx + (ex - sx) * t);
            float py = (sy + (ey - sy) * t);
            float dx = ex - sx;
            float dy = ey - sy;
            float angle = atan2f(dy, dx);
            // lateral lane offset: place car slightly to the right of its travel direction
            float laneOffset = (roadWidth * 0.25f);
            // determine dominant travel axis in screen space
            if (std::fabs(dx) > std::fabs(dy)) {
                // horizontal movement
                if (dx > 0.0f) {
                    // moving +x -> shift -y
                    py -= laneOffset;
                } else {
                    // moving -x -> shift +y
                    py += laneOffset;
                }
            } else {
                // vertical movement
                if (dy > 0.0f) {
                    // moving +y -> shift +x
                    px += laneOffset;
                } else {
                    // moving -y -> shift -x
                    px -= laneOffset;
                }
            }

            // car visual size (smaller)
            float cw = 12.0f;
            float ch = 6.0f;
            // draw outline (slightly larger, dark)
            drawRotatedRect(quadShader, px, py, cw + 4.0f, ch + 4.0f, angle, glm::vec3(0.05f, 0.05f, 0.05f));
            // draw car body
            drawRotatedRect(quadShader, px, py, cw, ch, angle, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        // ---- ImGui frame ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // grid heatmap overlay (draw using ImGui draw list so we don't change shader code)
        if (showHeatmap) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            const auto &hm = sim.getHeatMap();
            if (hm.width > 0 && hm.height > 0) {
                const int W = hm.width; const int H = hm.height;
                // find max cell value for normalization
                float maxVal = 0.0f;
                for (int i = 0; i < W * H; ++i) maxVal = std::max(maxVal, hm.cells[i]);
                if (maxVal > 1e-6f) {
                    float halfCellScreen = 0.5f * hm.cellSize * viewScale;
                    const float alpha = 0.35f;
                    for (int y = 0; y < H; ++y) {
                        for (int x = 0; x < W; ++x) {
                            float val = hm.cells[y * W + x];
                            if (val <= 0.0f) continue;
                            float t = val / maxVal;
                            if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                            ImVec4 col;
                            if (t <= 0.5f) {
                                float u = t * 2.0f;
                                col = ImVec4(0.0f, u, 1.0f - u, alpha);
                            } else {
                                float u = (t - 0.5f) * 2.0f;
                                col = ImVec4(u, 1.0f - u, 0.0f, alpha);
                            }
                            ImU32 icol = ImGui::ColorConvertFloat4ToU32(col);
                            // world center of cell
                            float wx = hm.originX + (x + 0.5f) * hm.cellSize;
                            float wy = hm.originY + (y + 0.5f) * hm.cellSize;
                            float sx = wx * viewScale + xOffset;
                            float sy = wy * viewScale + yOffset;
                            float yscreen = static_cast<float>(HEIGHT) - sy;
                            ImVec2 a(sx - halfCellScreen, yscreen - halfCellScreen);
                            ImVec2 b(sx + halfCellScreen, yscreen + halfCellScreen);
                            dl->AddRectFilled(a, b, icol);
                        }
                    }
                }
            }
        }

        // handle viewport interaction: left-click to select edge, right-drag to pan
        {
            ImGuiIO &io = ImGui::GetIO();
            // left click selection
            static bool prevLeft = false;
            bool leftPressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
            if (leftPressed && !prevLeft && !io.WantCaptureMouse) {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);
                float wx = static_cast<float>(mx);
                float wy = static_cast<float>(HEIGHT - my);
                const auto &net2 = sim.getNetwork();
                int best = -1;
                float bestDist = 1e9f;
                const float pickThreshold = 12.0f;
                for (size_t ei = 0; ei < net2.edges.size(); ++ei) {
                    const auto &e = net2.edges[ei];
                    if (!e.start || !e.end) continue;
                    float sx = e.start->position.x * viewScale + xOffset;
                    float sy = e.start->position.y * viewScale + yOffset;
                    float ex = e.end->position.x * viewScale + xOffset;
                    float ey = e.end->position.y * viewScale + yOffset;
                    float vx = ex - sx; float vy = ey - sy;
                    float wxs = wx - sx; float wys = wy - sy;
                    float len2 = vx*vx + vy*vy;
                    float t = (len2 > 0.0f) ? ((wxs*vx + wys*vy) / len2) : 0.0f;
                    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                    float px = sx + vx * t;
                    float py = sy + vy * t;
                    float dx = px - wx; float dy = py - wy;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < bestDist && dist <= pickThreshold) { bestDist = dist; best = static_cast<int>(ei); }
                }
                if (best >= 0) selectedEdgeIdx = best;
            }
            prevLeft = leftPressed;

            // right-drag panning
            static bool prevRight = false;
            static double lastMx = 0.0, lastMy = 0.0;
            bool rightPressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            if (rightPressed && !io.WantCaptureMouse) {
                if (prevRight) {
                    double dx = mx - lastMx;
                    double dy = my - lastMy;
                    xOffset += static_cast<float>(dx);
                    yOffset -= static_cast<float>(dy);
                }
                lastMx = mx; lastMy = my;
            }
            prevRight = rightPressed;
        }

        // increment frame counter and log only occasionally
        frameCounter++;
        if ((frameCounter % 300) == 0) {
            std::cout << "Frame " << frameCounter << " (periodic status)" << std::endl;
        }

        ImGui::Begin("Controls");
        ImGui::Text("Cars: %d", (int)sim.getCars().size());
        ImGui::Checkbox("Draw Models", &drawModels);
        ImGui::Checkbox("Show Nodes", &showNodes);
        ImGui::Checkbox("Show Lights", &showLights);
        ImGui::Checkbox("Show Stop Lines", &showStopLines);
        ImGui::Checkbox("Show Heatmap", &showHeatmap);
        // Traffic strategy selector
        static int strategyIdx = 1; // default to Density
        const char* strategyNames[] = { "Fixed Cycle", "Density", "Queue", "HeatMap", "Predictive", "Hybrid" };
        if (ImGui::Combo("Traffic Strategy", &strategyIdx, strategyNames, IM_ARRAYSIZE(strategyNames))) {
            sim.setControllerStrategyByIndex(strategyIdx);
        }
        ImGui::SliderFloat("View Scale", &viewScale, 1.0f, 8.0f);
        // Edge selector (preview)
        const auto &net = sim.getNetwork();
        std::vector<std::string> edgeLabels;
        std::vector<const char*> edgeItems;
        edgeLabels.reserve(net.edges.size());
        edgeItems.reserve(net.edges.size());
        for (size_t i = 0; i < net.edges.size(); ++i) {
            const auto &e = net.edges[i];
            char buf[128];
            int sid = e.start ? e.start->id : -1;
            int eid = e.end ? e.end->id : -1;
            std::snprintf(buf, sizeof(buf), "Edge %zu: %d -> %d", i, sid, eid);
            edgeLabels.emplace_back(buf);
            edgeItems.push_back(edgeLabels.back().c_str());
        }
        if (!edgeItems.empty()) {
            if (selectedEdgeIdx < 0 || selectedEdgeIdx >= static_cast<int>(edgeItems.size())) selectedEdgeIdx = 0;
            ImGui::Combo("Edge", &selectedEdgeIdx, edgeItems.data(), static_cast<int>(edgeItems.size()));
        } else {
            selectedEdgeIdx = -1;
            ImGui::Text("No edges available");
        }
        // Manual light control toggle
        static bool manualLights = false;
        if (ImGui::Checkbox("Manual Light Control", &manualLights)) {
            sim.manualLightControl = manualLights;
        }
        // If manual and an edge is selected, allow toggling allowed directions for its controlling light
        if (manualLights && selectedEdgeIdx >= 0 && selectedEdgeIdx < static_cast<int>(net.edges.size())) {
            // find light controlling this edge
            const auto &ctrl = sim.getController();
            int lightIndex = -1;
            for (size_t li = 0; li < ctrl.lights.size(); ++li) {
                if (ctrl.lights[li].controlledEdge == &net.edges[selectedEdgeIdx]) { lightIndex = static_cast<int>(li); break; }
            }
            if (lightIndex >= 0) {
                const auto &light = ctrl.lights[lightIndex];
                bool allowS = light.allowStraight;
                bool allowL = light.allowLeft;
                bool allowR = light.allowRight;
                if (ImGui::Checkbox("Allow Straight", &allowS) || ImGui::Checkbox("Allow Left", &allowL) || ImGui::Checkbox("Allow Right", &allowR)) {
                    sim.setLightAllowsForEdge(selectedEdgeIdx, allowS, allowL, allowR);
                } else {
                    // still show checkboxes without change
                    ImGui::SameLine();
                }
            } else {
                ImGui::Text("Selected edge has no traffic light");
            }
        }
        // turn probabilities
        float ps = sim.probStraight, pl = sim.probLeft, pr = sim.probRight;
        if (ImGui::SliderFloat("Prob Straight", &ps, 0.0f, 1.0f)) {}
        if (ImGui::SliderFloat("Prob Left", &pl, 0.0f, 1.0f)) {}
        if (ImGui::SliderFloat("Prob Right", &pr, 0.0f, 1.0f)) {}
        // normalize
        float ssum = ps + pl + pr;
        if (ssum > 0.0f) { sim.probStraight = ps/ssum; sim.probLeft = pl/ssum; sim.probRight = pr/ssum; }
        if (ImGui::Button(paused ? "Resume" : "Pause")) paused = !paused;
        ImGui::SameLine();
        if (ImGui::Button("Reset Simulation")) {
            sim.reset();
            sim.initializeIntersection();
            sim.update(0.016f);
        }
        ImGui::SameLine();
        ImGui::InputInt("Create Count", &imgui_create_count);
        if (imgui_create_count < 1) imgui_create_count = 1;
        if (ImGui::Button("Create Cars")) {
            // if an edge is selected, spawn on that edge; otherwise random
            sim.createCars(imgui_create_count, selectedEdgeIdx);
        }
        ImGui::SliderFloat("Simulation speed (%)", &globalSpeed, 10.0f, 500.0f);
        // apply simulation time scale
        sim.timeScale = globalSpeed / 100.0f;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        // update per-car speed to globalSpeed (optional)
        // Note: Simulation uses its own dynamics; globalSpeed currently unused for sim cars.

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        checkGLError("ImGui RenderDrawData");
        glfwSwapBuffers(window);
        checkGLError("glfwSwapBuffers");
        glfwPollEvents();
        // end of frame
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