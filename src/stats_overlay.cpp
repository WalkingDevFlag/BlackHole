#include "stats_overlay.h"
#include "imgui.h"
#include <GL/glew.h>  // Required for glGetString, glGetIntegerv, etc.

#ifdef __linux__
#include <sys/sysinfo.h>
#include <fstream>
#include <string>
#include <cstring> // For strstr
#include <cstdio>  // For popen, fgets, pclose
#include <sstream> // For stringstream
#endif

#ifdef USE_NVML
#include <nvml.h>
#endif

// Timer variables (using ImGui::GetTime)
static float lastUpdateTime = 0.0f;
static float updateInterval = 1.0f; // Update every ~1 second

// Variables for stats
static float currentFPS = 0.0f;
static int currentRAM = 0;
static int currentGPUUsage = 0;
static int currentTemp = 0;

#ifdef __linux__
// Retrieve RAM usage in MB using sysinfo.
static int GetRAMUsageMB()
{
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) == 0)
    {
        long long totalMem = memInfo.totalram * memInfo.mem_unit;
        long long freeMem  = memInfo.freeram  * memInfo.mem_unit;
        int usedMB = (totalMem - freeMem) / (1024 * 1024);
        return usedMB;
    }
    return 0;
}

// Original method using OpenGL extension (may not work under WSL)
static int GetGPUUsagePercent_OpenGL()
{
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensions && strstr(extensions, "GL_NVX_gpu_memory_info"))
    {
        int totalMemoryKB = 0;
        int currentAvailableKB = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalMemoryKB);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &currentAvailableKB);
        if (totalMemoryKB > 0)
        {
            int usedMemoryKB = totalMemoryKB - currentAvailableKB;
            return (usedMemoryKB * 100) / totalMemoryKB;
        }
    }
    return 0;
}

#ifdef USE_NVML
// Retrieve GPU usage percentage using NVML.
static int GetNVMLGPUUsagePercent()
{
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS)
        return 0;
    nvmlDevice_t device;
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (result != NVML_SUCCESS) {
        nvmlShutdown();
        return 0;
    }
    nvmlUtilization_t utilization;
    result = nvmlDeviceGetUtilizationRates(device, &utilization);
    if (result != NVML_SUCCESS) {
        nvmlShutdown();
        return 0;
    }
    int usage = utilization.gpu;
    nvmlShutdown();
    return usage;
}

// Retrieve GPU temperature using NVML.
static int GetNVMLGPUTemperature()
{
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS)
        return 0;
    nvmlDevice_t device;
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (result != NVML_SUCCESS) {
        nvmlShutdown();
        return 0;
    }
    unsigned int temp;
    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp);
    if (result != NVML_SUCCESS) {
        nvmlShutdown();
        return 0;
    }
    nvmlShutdown();
    return static_cast<int>(temp);
}
#endif // USE_NVML

// Alternative method: query using nvidia-smi via popen.
#ifdef __linux__
static int GetGPUUsagePercent_Smi()
{
    FILE* pipe = popen("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits", "r");
    if (!pipe)
        return 0;
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
    {
        result += buffer;
    }
    pclose(pipe);
    try {
        return std::stoi(result);
    } catch(...) {
        return 0;
    }
}

static int GetGPUTemperature_Smi()
{
    FILE* pipe = popen("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits", "r");
    if (!pipe)
        return 0;
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
    {
        result += buffer;
    }
    pclose(pipe);
    try {
        return std::stoi(result);
    } catch(...) {
        return 0;
    }
}
#endif

// Retrieve CPU temperature by reading from sysfs (in millidegrees Celsius).
static int GetCPUTemperature()
{
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (file.is_open())
    {
        int temp;
        file >> temp;
        file.close();
        return temp / 1000; // Convert to degrees Celsius.
    }
    return 0;
}
#endif

static void UpdateStats()
{
    float currentTime = ImGui::GetTime();
    if (currentTime - lastUpdateTime > updateInterval)
    {
        currentFPS = ImGui::GetIO().Framerate;
#ifdef __linux__
        currentRAM = GetRAMUsageMB();
#ifdef USE_NVML
        int gpuUsage = GetNVMLGPUUsagePercent();
        int gpuTemp = GetNVMLGPUTemperature();
        if (gpuUsage == 0)
            gpuUsage = GetGPUUsagePercent_Smi();
        if (gpuTemp == 0)
            gpuTemp = GetGPUTemperature_Smi();
        currentGPUUsage = gpuUsage;
        currentTemp = gpuTemp;
#else
        currentGPUUsage = GetGPUUsagePercent_OpenGL();
        currentTemp = GetCPUTemperature();
#endif
#else
        currentRAM = 4096;
        currentGPUUsage = 70;
        currentTemp = 55;
#endif
        lastUpdateTime = currentTime;
    }
}

void RenderStatsOverlay()
{
    UpdateStats();

    // Position the overlay in the top-right corner.
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x - 10, 10),
        ImGuiCond_Always,
        ImVec2(1, 0)
    );

    ImGui::Begin("Stats Overlay", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove);

    ImGui::Text("FPS: %.1f", currentFPS);
    ImGui::Text("RAM: %d MB", currentRAM);
    ImGui::Text("GPU Usage: %d%%", currentGPUUsage);
    ImGui::Text("Temp: %d C", currentTemp);

    // Debug: Display last update time to verify refreshing.
    // ImGui::Text("Last update: %.1f", lastUpdateTime);

    ImGui::End();
}