// cuda_simulation.cu
#include "cuda_simulation.h"
#include <iostream>
#include <vector>
#include <cuda_runtime.h>
#include <chrono>

// CUDA kernel for vector addition
__global__ void vecAdd(const float* A, const float* B, float* C, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if(i < n)
        C[i] = A[i] + B[i];
}

void runCUDASimulation() {
    const int n = 1024;
    size_t size = n * sizeof(float);
    std::vector<float> h_A(n, 1.0f), h_B(n, 2.0f), h_C(n, 0.0f);
    
    float *d_A, *d_B, *d_C;
    cudaError_t err;
    
    // Allocate device memory
    err = cudaMalloc((void**)&d_A, size);
    if(err != cudaSuccess) { std::cerr << "cudaMalloc failed for d_A\n"; return; }
    err = cudaMalloc((void**)&d_B, size);
    if(err != cudaSuccess) { std::cerr << "cudaMalloc failed for d_B\n"; cudaFree(d_A); return; }
    err = cudaMalloc((void**)&d_C, size);
    if(err != cudaSuccess) { std::cerr << "cudaMalloc failed for d_C\n"; cudaFree(d_A); cudaFree(d_B); return; }
    
    // Copy input data to device
    err = cudaMemcpy(d_A, h_A.data(), size, cudaMemcpyHostToDevice);
    if(err != cudaSuccess) { std::cerr << "cudaMemcpy failed for d_A\n"; return; }
    err = cudaMemcpy(d_B, h_B.data(), size, cudaMemcpyHostToDevice);
    if(err != cudaSuccess) { std::cerr << "cudaMemcpy failed for d_B\n"; return; }
    
    // Launch kernel: choose block and grid sizes
    int threadsPerBlock = 256;
    int blocksPerGrid = (n + threadsPerBlock - 1) / threadsPerBlock;
    
    auto start = std::chrono::high_resolution_clock::now();
    vecAdd<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, n);
    cudaDeviceSynchronize();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> execTime = end - start;
    std::cout << "CUDA kernel execution time: " << execTime.count() << " ms\n";
    
    // Copy result back to host
    err = cudaMemcpy(h_C.data(), d_C, size, cudaMemcpyDeviceToHost);
    if(err != cudaSuccess) { std::cerr << "cudaMemcpy failed for result\n"; }
    
    // Check results (optional)
    bool success = true;
    for (int i = 0; i < n; i++) {
        if(h_C[i] != 3.0f) { success = false; break; }
    }
    std::cout << "CUDA Simulation: " << (success ? "Success!" : "Failure!") << std::endl;
    
    // Clean up device memory
    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);
}