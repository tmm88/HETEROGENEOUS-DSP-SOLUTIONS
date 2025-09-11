// Example Vitis HLS C++ code for synthesizing the audio generator to FPGA RTL.
// This is a top-level function that generates a buffer of mixed sine sweep samples.
// Host code (not shown) would generate random freqs, call the kernel via Vitis API, stream to audio.
// Use fixed-point for better FPGA efficiency; here using float for simplicity.
// Include <hls_math.h> for sinf().

#include <hls_stream.h>
#include <hls_math.h>
#include <ap_fixed.h>

#define NUM_OSC 32
#define PI 3.141592653589793f

// Top function for HLS
extern "C" {
void audio_synth(
    float* starts,  // Input array of 32 start frequencies
    float* ends,    // Input array of 32 end frequencies
    float* output,  // Output mixed audio buffer
    int num_samples,  // Number of samples to generate (e.g., 60 * sample_rate)
    float sample_rate // Sample rate (e.g., 44100.0)
) {
#pragma HLS INTERFACE m_axi port=starts offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=ends offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem2
#pragma HLS INTERFACE s_axilite port=num_samples bundle=control
#pragma HLS INTERFACE s_axilite port=sample_rate bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    float phases[NUM_OSC];
    for (int osc = 0; osc < NUM_OSC; ++osc) {
#pragma HLS UNROLL
        phases[osc] = 0.0f;
    }

gen_loop:
    for (int i = 0; i < num_samples; ++i) {
#pragma HLS PIPELINE II=1
        float t = (float)i / sample_rate;
        float mix = 0.0f;

        for (int osc = 0; osc < NUM_OSC; ++osc) {
#pragma HLS UNROLL
            float freq = starts[osc] + (ends[osc] - starts[osc]) * (t / 60.0f);
            phases[osc] += 2.0f * PI * freq / sample_rate;
            mix += sinf(phases[osc]) * 0.06f;
        }

        output[i] = mix;
    }
}
}

// Note: For better performance, use ap_fixed<16,1> for phases/freq, and LUT or CORDIC for sin instead of sinf().