// OpenCL for FPGAs is already a form of HLS, similar to C/C++ HLS or MyHDL.
// The previous OpenCL kernel provided is suitable; it can be compiled with tools like Intel FPGA OpenCL SDK or Xilinx SDAccel/Vitis.
// For similarity to HLS/MyHDL, note that OpenCL kernels describe parallel hardware, like MyHDL modules or HLS functions with pragmas.
// No major changes needed, but here's an adjusted version with fixed-point for better FPGA fit.

// Adjusted Kernel
#define NUM_OSC 32
#define PI 3.141592653589793f
#define AMP 0.06f
#define FIXED_WIDTH 16
#define FIXED_FRAC 8
typedef ap_fixed<FIXED_WIDTH, FIXED_FRAC> fixed_t;  // Use vendor fixed-point if needed

__kernel void synth_sweeps(
    __global const float* starts,
    __global const float* ends,
    const float time_offset,
    const int buffer_size,
    const int sample_rate,
    __global float* output,
    __global float* phases
) {
    int gid = get_global_id(0);

    if (gid >= buffer_size) return;

    float t = time_offset + (float)gid / sample_rate;
    float mix = 0.0f;

#pragma unroll
    for (int osc = 0; osc < NUM_OSC; ++osc) {
        float freq = starts[osc] + (ends[osc] - starts[osc]) * (t / 60.0f);
        phases[osc] += 2 * PI * freq / sample_rate;
        mix += sin(phases[osc]) * AMP;
    }

    output[gid] = mix;
}

// For better efficiency, replace sin with LUT or CORDIC intrinsic.