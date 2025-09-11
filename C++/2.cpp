#include <hls_math.h>
#include <ap_fixed.h>

// Define fixed-point types for FPGA efficiency
typedef ap_fixed<16, 4> fixed_t; // 16-bit fixed-point, 4 integer bits
const int NUM_OSC = 32;
const float SAMPLE_RATE = 48000.0; // Audio sample rate (Hz)
const float TWO_PI = 6.28318530718;

// Precomputed random frequencies and LFO rates (generated offline)
fixed_t osc_freq[NUM_OSC] = { /* e.g., 20, 50, 100, ..., 2000 Hz */ };
fixed_t lfo_freq[NUM_OSC] = { /* e.g., 0.01, 0.02, ..., 0.1 Hz */ };
fixed_t amplitude = 0.01;

// HLS function to generate one sample
#pragma hls_top
void tone_generator(fixed_t &output_sample) {
    #pragma HLS PIPELINE II=1 // Pipeline for one sample per clock cycle
    fixed_t sum = 0.0;
    static fixed_t phase[NUM_OSC] = {0}; // Phase accumulators for oscillators
    static fixed_t lfo_phase[NUM_OSC] = {0}; // Phase accumulators for LFOs

    // Parallel loop for 32 oscillators
    #pragma HLS UNROLL factor=32
    for (int i = 0; i < NUM_OSC; i++) {
        // Update oscillator phase (phase += 2π * freq / sample_rate)
        phase[i] += TWO_PI * osc_freq[i] / SAMPLE_RATE;
        if (phase[i] >= TWO_PI) phase[i] -= TWO_PI;

        // Update LFO phase
        lfo_phase[i] += TWO_PI * lfo_freq[i] / SAMPLE_RATE;
        if (lfo_phase[i] >= TWO_PI) lfo_phase[i] -= TWO_PI;

        // Compute sine wave with LFO modulation
        fixed_t lfo = hls::sin(lfo_phase[i]); // LFO modulates amplitude
        fixed_t osc = hls::sin(phase[i]); // Carrier oscillator
        sum += osc * lfo * amplitude;
    }

    output_sample = sum; // Output summed waveform
}

// Initialization of random frequencies (example, replace with actual random values)
void init_frequencies() {
    for (int i = 0; i < NUM_OSC; i++) {
        osc_freq[i] = 20.0 + (fixed_t)(i * 1980.0 / NUM_OSC); // Linearly spaced 20-2000 Hz
        lfo_freq[i] = 0.01 + (fixed_t)(i * 0.09 / NUM_OSC); // Linearly spaced 0.01-0.1 Hz
    }
}