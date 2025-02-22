/**
 * @file main.cpp
 * @author Ross 
 * @brief Real-time black hole rendering in OpenGL.
 * @version 0.1
 * @date 2020-08-29
 *
 * @copyright Copyright (c) 2020
 *
 */

 #include <assert.h>
 #include <map>
 #include <stdio.h>
 #include <vector>
 
 #include <GL/glew.h>
 #include <GLFW/glfw3.h>
 #include <glm/glm.hpp>
 #include <glm/gtc/matrix_transform.hpp>
 #include <glm/gtc/type_ptr.hpp>
 #include <glm/gtx/euler_angles.hpp>
 #include <imgui.h>
 
 // Added for asynchronous simulation updates and heavy computation
 #include <thread>
 #include <mutex>
 #include <chrono>
 #include <cmath>
 
 #include <GLDebugMessageCallback.h>
 #include <imgui_impl_glfw.h>
 #include <imgui_impl_opengl3.h>
 #include <render.h>
 #include <shader.h>
 #include <texture.h>
 
 #include "stats_overlay.h"
 
 static const int SCR_WIDTH = 1920;
 static const int SCR_HEIGHT = 1080;
 
 static float mouseX, mouseY;
 
 #define IMGUI_TOGGLE(NAME, DEFAULT)                                            \
   static bool NAME = DEFAULT;                                                  \
   ImGui::Checkbox(#NAME, &NAME);                                               \
   rtti.floatUniforms[#NAME] = NAME ? 1.0f : 0.0f;
 
 #define IMGUI_SLIDER(NAME, DEFAULT, MIN, MAX)                                  \
   static float NAME = DEFAULT;                                                 \
   ImGui::SliderFloat(#NAME, &NAME, MIN, MAX);                                  \
   rtti.floatUniforms[#NAME] = NAME;
 
 // Global simulation data
 std::mutex simulationMutex;
 float simulationParam = 0.0f; // Updated by simulation thread (not used by shader anymore)
 bool simulationRunning = true;
 
 // Simulation thread function with heavy (dummy) computation.
 // This offloads intensive work from the main thread.
 void simulationThreadFunc() {
   using namespace std::chrono;
   auto lastTime = steady_clock::now();
   while (simulationRunning) {
     auto now = steady_clock::now();
     duration<float> delta = now - lastTime;
     lastTime = now;
     float dt = delta.count();
     float speed = 0.5f; // simulation speed factor
 
     {
       std::lock_guard<std::mutex> lock(simulationMutex);
       simulationParam += dt * speed;
       if (simulationParam > 6.28318f) // wrap around 2*pi
         simulationParam -= 6.28318f;
     }
 
     // Simulate heavy computation to mimic a demanding simulation.
     volatile double dummy = 0.0;
     for (int i = 0; i < 1000000; i++) {
       dummy += sin(i * simulationParam);
     }
 
     std::this_thread::sleep_for(milliseconds(10));
   }
 }
 
 static void glfwErrorCallback(int error, const char *description) {
   fprintf(stderr, "Glfw Error %d: %s\n", error, description);
 }
 
 void mouseCallback(GLFWwindow *window, double x, double y) {
   static float lastX = 400.0f;
   static float lastY = 300.0f;
   static float yaw = 0.0f;
   static float pitch = 0.0f;
   static float firstMouse = true;
 
   mouseX = (float)x;
   mouseY = (float)y;
 }
 
 class PostProcessPass {
 private:
   GLuint program;
 
 public:
   PostProcessPass(const std::string &fragShader) {
     this->program = createShaderProgram("shader/simple.vert", fragShader);
 
     glUseProgram(this->program);
     glUniform1i(glGetUniformLocation(program, "texture0"), 0);
     glUseProgram(0);
   }
 
   void render(GLuint inputColorTexture, GLuint destFramebuffer = 0) {
     glBindFramebuffer(GL_FRAMEBUFFER, destFramebuffer);
 
     glDisable(GL_DEPTH_TEST);
 
     glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
     glClear(GL_COLOR_BUFFER_BIT);
 
     glUseProgram(this->program);
 
     glUniform2f(glGetUniformLocation(this->program, "resolution"),
                 (float)SCR_WIDTH, (float)SCR_HEIGHT);
 
     glUniform1f(glGetUniformLocation(this->program, "time"),
                 (float)glfwGetTime());
 
     glActiveTexture(GL_TEXTURE0);
     glBindTexture(GL_TEXTURE_2D, inputColorTexture);
 
     glDrawArrays(GL_TRIANGLES, 0, 6);
 
     glUseProgram(0);
   }
 };
 
 int main(int, char **) {
   // Setup window
   glfwSetErrorCallback(glfwErrorCallback);
   if (!glfwInit())
     return 1;
 
   glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
   GLFWwindow *window =
       glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Wormhole", NULL, NULL);
   if (window == NULL)
     return 1;
   glfwMakeContextCurrent(window);
   glfwSwapInterval(1); // Enable vsync
   glfwSetCursorPosCallback(window, mouseCallback);
   glfwSetWindowPos(window, 0, 0);
 
   bool err = glewInit() != GLEW_OK;
   if (err) {
     fprintf(stderr, "Failed to initialize OpenGL loader!\n");
     return 1;
   }
 
   if (0)
   {
     glEnable(GL_DEBUG_OUTPUT);
     glDebugMessageCallback(GLDebugMessageCallback, nullptr);
   }
 
   {
 #if __APPLE__
     const char *glsl_version = "#version 150";
     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
     glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
     glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
 #else
     const char *glsl_version = "#version 130";
     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
 #endif
 
     IMGUI_CHECKVERSION();
     ImGui::CreateContext();
     ImGuiIO &io = ImGui::GetIO();
     (void)io;
 
     ImGui::StyleColorsDark();
 
     ImGui_ImplGlfw_InitForOpenGL(window, true);
     ImGui_ImplOpenGL3_Init(glsl_version);
 
     bool show_demo_window = true;
     bool show_another_window = false;
     ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
   }
 
   // Start the simulation thread to offload heavy simulation computations.
   std::thread simulationThread(simulationThreadFunc);
 
   GLuint fboBlackhole, texBlackhole;
   texBlackhole = createColorTexture(SCR_WIDTH, SCR_HEIGHT);
 
   FramebufferCreateInfo info = {};
   info.colorTexture = texBlackhole;
   if (!(fboBlackhole = createFramebuffer(info))) {
     assert(false);
   }
 
   GLuint quadVAO = createQuadVAO();
   glBindVertexArray(quadVAO);
 
   PostProcessPass passthrough("shader/passthrough.frag");
 
   while (!glfwWindowShouldClose(window)) {
     glfwPollEvents();
 
     ImGui_ImplOpenGL3_NewFrame();
     ImGui_ImplGlfw_NewFrame();
     ImGui::NewFrame();
 
     int width, height;
     glfwGetFramebufferSize(window, &width, &height);
     glViewport(0, 0, width, height);
 
     glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 
     static GLuint galaxy = loadCubemap("assets/skybox_nebula_dark");
     static GLuint colorMap = loadTexture2D("assets/color_map.png");
     static GLuint uvChecker = loadTexture2D("assets/uv_checker.png");
 
     static GLuint texBlackhole = createColorTexture(SCR_WIDTH, SCR_HEIGHT);
     {
       RenderToTextureInfo rtti;
       rtti.fragShader = "shader/blackhole_main.frag";
       rtti.cubemapUniforms["galaxy"] = galaxy;
       rtti.textureUniforms["colorMap"] = colorMap;
       rtti.floatUniforms["mouseX"] = mouseX;
       rtti.floatUniforms["mouseY"] = mouseY;
       // Removed simulationParam uniform update to avoid warnings since it's not used in the shader.
       rtti.targetTexture = texBlackhole;
       rtti.width = SCR_WIDTH;
       rtti.height = SCR_HEIGHT;
 
       IMGUI_TOGGLE(gravatationalLensing, true);
       IMGUI_TOGGLE(renderBlackHole, true);
       IMGUI_TOGGLE(mouseControl, true);
       IMGUI_SLIDER(cameraRoll, 0.0f, -180.0f, 180.0f);
       IMGUI_TOGGLE(frontView, false);
       IMGUI_TOGGLE(topView, false);
       IMGUI_TOGGLE(adiskEnabled, true);
       IMGUI_TOGGLE(adiskParticle, true);
       IMGUI_SLIDER(adiskDensityV, 2.0f, 0.0f, 10.0f);
       IMGUI_SLIDER(adiskDensityH, 4.0f, 0.0f, 10.0f);
       IMGUI_SLIDER(adiskHeight, 0.55f, 0.0f, 1.0f);
       IMGUI_SLIDER(adiskLit, 0.25f, 0.0f, 4.0f);
       IMGUI_SLIDER(adiskNoiseLOD, 5.0f, 1.0f, 12.0f);
       IMGUI_SLIDER(adiskNoiseScale, 0.8f, 0.0f, 10.0f);
       IMGUI_SLIDER(adiskSpeed, 0.5f, 0.0f, 1.0f);
 
       renderToTexture(rtti);
     }
 
     static GLuint texBrightness = createColorTexture(SCR_WIDTH, SCR_HEIGHT);
     {
       RenderToTextureInfo rtti;
       rtti.fragShader = "shader/bloom_brightness_pass.frag";
       rtti.textureUniforms["texture0"] = texBlackhole;
       rtti.targetTexture = texBrightness;
       rtti.width = SCR_WIDTH;
       rtti.height = SCR_HEIGHT;
       renderToTexture(rtti);
     }
 
     const int MAX_BLOOM_ITER = 8;
     static GLuint texDownsampled[MAX_BLOOM_ITER];
     static GLuint texUpsampled[MAX_BLOOM_ITER];
     if (texDownsampled[0] == 0) {
       for (int i = 0; i < MAX_BLOOM_ITER; i++) {
         texDownsampled[i] =
             createColorTexture(SCR_WIDTH >> (i + 1), SCR_HEIGHT >> (i + 1));
         texUpsampled[i] = createColorTexture(SCR_WIDTH >> i, SCR_HEIGHT >> i);
       }
     }
 
     static int bloomIterations = MAX_BLOOM_ITER;
     ImGui::SliderInt("bloomIterations", &bloomIterations, 1, 8);
     for (int level = 0; level < bloomIterations; level++) {
       RenderToTextureInfo rtti;
       rtti.fragShader = "shader/bloom_downsample.frag";
       rtti.textureUniforms["texture0"] =
           level == 0 ? texBrightness : texDownsampled[level - 1];
       rtti.targetTexture = texDownsampled[level];
       rtti.width = SCR_WIDTH >> (level + 1);
       rtti.height = SCR_HEIGHT >> (level + 1);
       renderToTexture(rtti);
     }
 
     for (int level = bloomIterations - 1; level >= 0; level--) {
       RenderToTextureInfo rtti;
       rtti.fragShader = "shader/bloom_upsample.frag";
       rtti.textureUniforms["texture0"] = level == bloomIterations - 1
                                              ? texDownsampled[level]
                                              : texUpsampled[level + 1];
       rtti.textureUniforms["texture1"] =
           level == 0 ? texBrightness : texDownsampled[level - 1];
       rtti.targetTexture = texUpsampled[level];
       rtti.width = SCR_WIDTH >> level;
       rtti.height = SCR_HEIGHT >> level;
       renderToTexture(rtti);
     }
 
     static GLuint texBloomFinal = createColorTexture(SCR_WIDTH, SCR_HEIGHT);
     {
       RenderToTextureInfo rtti;
       rtti.fragShader = "shader/bloom_composite.frag";
       rtti.textureUniforms["texture0"] = texBlackhole;
       rtti.textureUniforms["texture1"] = texUpsampled[0];
       rtti.targetTexture = texBloomFinal;
       rtti.width = SCR_WIDTH;
       rtti.height = SCR_HEIGHT;
 
       IMGUI_SLIDER(bloomStrength, 0.1f, 0.0f, 1.0f);
 
       renderToTexture(rtti);
     }
 
     static GLuint texTonemapped = createColorTexture(SCR_WIDTH, SCR_HEIGHT);
     {
       RenderToTextureInfo rtti;
       rtti.fragShader = "shader/tonemapping.frag";
       rtti.textureUniforms["texture0"] = texBloomFinal;
       rtti.targetTexture = texTonemapped;
       rtti.width = SCR_WIDTH;
       rtti.height = SCR_HEIGHT;
 
       IMGUI_TOGGLE(tonemappingEnabled, true);
       IMGUI_SLIDER(gamma, 2.5f, 1.0f, 4.0f);
 
       renderToTexture(rtti);
     }
 
     passthrough.render(texTonemapped);
 
     // Render the stats overlay
     RenderStatsOverlay();
 
     ImGui::Render();
     ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
 
     glfwSwapBuffers(window);
   }
 
   simulationRunning = false;
   simulationThread.join();
 
   glfwDestroyWindow(window);
   glfwTerminate();
 
   return 0;
 }
 