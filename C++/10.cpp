#include <hls_stream.h>  // For dataflow

typedef float data_t;  // Fixed-point or float for DSP

extern "C" void fm_synth_top(hls::stream<data_t>& in_stream, hls::stream<data_t>& out_left, hls::stream<data_t>& out_right) {
    #pragma HLS INTERFACE axis port=in_stream  // AXI-Stream for input (e.g., clock)
    #pragma HLS INTERFACE axis port=out_left
    #pragma HLS INTERFACE axis port=out_right
    #pragma HLS DATAFLOW  // Enable pipelining

    const int num_osc = 8;
    const float sample_rate = 44100.0f;
    const float mod_depth = 1.0f / (8 * 4);
    float base_freq = 47.0f + (rand() % 100) / 100.0f;  // Pseudo-random

    data_t mixed = 0.0f;
    for (int osc = 0; osc < num_osc; ++osc) {
        #pragma HLS UNROLL  // Parallelize oscillators
        float lfo_freq = 3.14159f * (0.5f + rand() % 2);  // Choose from [1.57, 3.14, 6.28]
        // LFO and phase accumulation logic here (cumulative sin for FM)
        // ... (integrate modulated_freq over time)
        data_t osc_out = sin(phase);  // Simplified
        mixed += osc_out;
    }
    mixed /= num_osc;
    out_left.write(mixed);  // Stereo duplicate
    out_right.write(mixed);
}
