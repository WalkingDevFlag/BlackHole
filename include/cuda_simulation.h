#ifndef CUDA_SIMULATION_H
#define CUDA_SIMULATION_H

#ifdef __cplusplus
extern "C" {
#endif

// Host function that runs the CUDA simulation in its own thread.
// This function launches a CUDA kernel that computes a heavy simulation
// (summing sin(i * simulationParam) over many iterations) and prints the result.
void runCUDASimulation();

#ifdef __cplusplus
}
#endif

#endif // CUDA_SIMULATION_H
