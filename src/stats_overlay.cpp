#include "stats_overlay.h"
#include "imgui.h"
#include <GL/glew.h>  // Required for glGetString, glGetIntegerv, etc.
#include <string>
#include <cstring> // For strstr

#ifdef __linux__
#include <sys/sysinfo.h>
#include <fstream>
#include <sstream>
#include <cstdio>  // For popen, fgets, pclose
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

// Helper function to get the GPU vendor from the OpenGL renderer string.
static std::string GetGPUVendor()
{
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return renderer ? std::string(renderer) : "";
}

// -------------------- NVIDIA Functions --------------------
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

// Fallback: Query NVIDIA metrics via nvidia-smi.
static int GetGPUUsagePercent_NvidiaSmi()
{
#ifdef __linux__
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
#else
    return 0;
#endif
}

static int GetGPUTemperature_NvidiaSmi()
{
#ifdef __linux__
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
#else
    return 0;
#endif
}

// -------------------- AMD Functions (Stub) --------------------
// For AMD GPUs, you might need to use AMD ADL (Windows) or parse DRM/sysfs (Linux).
static int GetGPUUsagePercent_AMD()
{
    // TODO: Implement AMD-specific metric retrieval.
    return 0;
}

static int GetGPUTemperature_AMD()
{
    // TODO: Implement AMD-specific metric retrieval.
    return 0;
}

// -------------------- Intel Functions (Stub) --------------------
// For Intel GPUs, you may query DRM statistics or sysfs entries.
static int GetGPUUsagePercent_Intel()
{
    // TODO: Implement Intel-specific metric retrieval.
    return 0;
}

static int GetGPUTemperature_Intel()
{
    // TODO: Implement Intel-specific metric retrieval.
    return 0;
}

// -------------------- End of Vendor-specific Functions --------------------

#endif // __linux__

static void UpdateStats()
{
    float currentTime = ImGui::GetTime();
    if (currentTime - lastUpdateTime > updateInterval)
    {
        currentFPS = ImGui::GetIO().Framerate;
#ifdef __linux__
        currentRAM = GetRAMUsageMB();

        // Detect vendor and choose appropriate functions.
        std::string vendor = GetGPUVendor();
        if (vendor.find("NVIDIA") != std::string::npos)
        {
#ifdef USE_NVML
            int gpuUsage = GetNVMLGPUUsagePercent();
            int gpuTemp = GetNVMLGPUTemperature();
            if (gpuUsage == 0)
                gpuUsage = GetGPUUsagePercent_NvidiaSmi();
            if (gpuTemp == 0)
                gpuTemp = GetGPUTemperature_NvidiaSmi();
            currentGPUUsage = gpuUsage;
            currentTemp = gpuTemp;
#else
            currentGPUUsage = GetGPUUsagePercent_NvidiaSmi();
            currentTemp = GetGPUTemperature_NvidiaSmi();
#endif
        }
        else if (vendor.find("AMD") != std::string::npos)
        {
            currentGPUUsage = GetGPUUsagePercent_AMD();
            currentTemp = GetGPUTemperature_AMD();
        }
        else if (vendor.find("Intel") != std::string::npos)
        {
            currentGPUUsage = GetGPUUsagePercent_Intel();
            currentTemp = GetGPUTemperature_Intel();
        }
        else
        {
            currentGPUUsage = 0;
            currentTemp = 0;
        }
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

    // Debug: show last update time to verify refreshing.
    // ImGui::Text("Last update: %.1f", lastUpdateTime);

    ImGui::End();
}
