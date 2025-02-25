#include <cuda_runtime.h>
#include <math.h>
#include <stdio.h>
#include "cuda_simulation.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// CUDA Kernel: Each thread computes a partial sum of sin(i * simulationParam)
// over a subset of iterations. The results are combined using an atomic addition.
__global__ void simulationKernel(float simulationParam, int iterations, float *result) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    float sum = 0.0f;
    for (int i = idx; i < iterations; i += stride) {
        sum += sinf(i * simulationParam);
    }
    // Use atomic addition to accumulate the partial sum.
    atomicAdd(result, sum);
}

// Host function that runs the CUDA simulation.
extern "C" void runCUDASimulation() {
    const int iterations = 1000000; // Total iterations per simulation step.
    float h_result = 0.0f;
    float *d_result;
    
    // Allocate device memory for the result.
    cudaMalloc((void**)&d_result, sizeof(float));
    cudaMemcpy(d_result, &h_result, sizeof(float), cudaMemcpyHostToDevice);

    const int threadsPerBlock = 256;
    int blocks = (iterations + threadsPerBlock - 1) / threadsPerBlock;
    float simulationParam = 0.0f;

    while (true) {
        // Update simulation parameter and wrap around 2π.
        simulationParam = fmodf(simulationParam + 0.01f, 6.28318f);
        
        // Reset the device result.
        cudaMemset(d_result, 0, sizeof(float));
        
        // Launch the kernel.
        simulationKernel<<<blocks, threadsPerBlock>>>(simulationParam, iterations, d_result);
        cudaDeviceSynchronize();
        
        // Copy the result back from device to host.
        cudaMemcpy(&h_result, d_result, sizeof(float), cudaMemcpyDeviceToHost);
        printf("CUDA Simulation result: %f, simulationParam: %f\n", h_result, simulationParam);
        
        // Sleep for 10ms to mimic timestep pacing.
        #ifdef _WIN32
            Sleep(10);
        #else
            usleep(10 * 1000); // 10ms on Unix-like systems.
        #endif
    }

    // Cleanup (unreachable in this infinite loop; add termination logic as needed).
    cudaFree(d_result);
}
