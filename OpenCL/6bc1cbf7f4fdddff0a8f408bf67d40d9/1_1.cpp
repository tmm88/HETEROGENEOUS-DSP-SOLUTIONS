// OpenCL kernel for tone generator
__kernel void tone_generator(__global float *output, __global float *osc_freq, __global float *lfo_freq, __global float *phase, __global float *lfo_phase) {
    const int NUM_OSC = 32;
    const float SAMPLE_RATE = 48000.0f;
    const float TWO_PI = 6.28318530718f;
    const float AMPLITUDE = 0.01f;

    // Get global ID for parallel processing
    int gid = get_global_id(0);
    float sum = 0.0f;

    // Process all 32 oscillators in parallel
    if (gid == 0) {
        #pragma unroll 32
        for (int i = 0; i < NUM_OSC; i++) {
            // Update oscillator phase
            phase[i] += TWO_PI * osc_freq[i] / SAMPLE_RATE;
            if (phase[i] >= TWO_PI) phase[i] -= TWO_PI;

            // Update LFO phase
            lfo_phase[i] += TWO_PI * lfo_freq[i] / SAMPLE_RATE;
            if (lfo_phase[i] >= TWO_PI) lfo_phase[i] -= TWO_PI;

            // Compute sine wave with LFO modulation
            float lfo = sin(lfo_phase[i]); // LFO modulates amplitude
            float osc = sin(phase[i]);     // Carrier oscillator
            sum += osc * lfo * AMPLITUDE;
        }
        output[0] = sum; // Write summed sample
    }
}