#include "stats_overlay.h"
#include "imgui.h"

#ifdef __linux__
#include <sys/sysinfo.h>
#include <fstream>
#include <string>
#endif

// Timer variables (using ImGui::GetTime)
static float lastUpdateTime = 0.0f;
static float updateInterval = 1.0f; // Update every ~1 seconds

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

#ifdef GL_NVX_gpu_memory_info
// Retrieve GPU usage percentage using the GL_NVX_gpu_memory_info extension.
static int GetGPUUsagePercent()
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
    return 0;
}
#else
static int GetGPUUsagePercent()
{
    return 0;
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
        currentGPUUsage = GetGPUUsagePercent();
        currentTemp = GetCPUTemperature();
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
    // We set the position using a pivot of (1,0) so the window's top-right corner is anchored.
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

    ImGui::End();
}
