#include "ap_fixed.h"
#include "hls_stream.h"
#include "hls_math.h"

#define NUM_INST 16
#define SR 44100
#define REVERB_SIZE 22050  // Half second delay for reverb

void synth(hls::stream<ap_fixed<16,4>> &out_stream) {
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL
    #pragma HLS INTERFACE axis port=out_stream
    #pragma HLS PIPELINE II=1

    static ap_fixed<16,4> phase[NUM_INST] = {0};
    #pragma HLS ARRAY_PARTITION variable=phase complete dim=1

    static ap_fixed<16,4> env[NUM_INST] = {0};
    #pragma HLS ARRAY_PARTITION variable=env complete dim=1

    static ap_fixed<16,4> reverb_buffer[REVERB_SIZE] = {0};
    static int reverb_idx = 0;

    static int trigger_counter = 0;
    const int trigger_rate = SR / 10;  // Simplified trigger ~10 Hz instead of LFDNoise0

    ap_fixed<16,4> sum = 0;

    for (int i = 0; i < NUM_INST; i++) {
        #pragma HLS UNROLL

        ap_fixed<16,4> freq = 32 + i;  // Fixed frequencies around 32-48 Hz

        if (trigger_counter == 0) {
            env[i] = 0;  // Reset and trigger attack
        }

        phase[i] += 2 * M_PI * freq / SR;
        if (phase[i] > 2 * M_PI) phase[i] -= 2 * M_PI;

        ap_fixed<16,4> osc = hls::sinf(phase[i]);

        // Simple perc envelope: attack 0.01s, release 1s
        if (env[i] < 1.0) {
            env[i] += 100.0 / SR;  // Attack rate
        } else {
            env[i] -= 1.0 / SR;  // Release rate
            if (env[i] < 0) env[i] = 0;
        }

        ap_fixed<16,4> signal = osc * env[i];

        // Simple reverb: single delay with feedback
        ap_fixed<16,4> rev = reverb_buffer[reverb_idx];
        reverb_buffer[reverb_idx] = signal + rev * 0.7;  // Feedback 0.7
        sum += rev / NUM_INST;
    }

    reverb_idx = (reverb_idx + 1) % REVERB_SIZE;
    trigger_counter = (trigger_counter + 1) % trigger_rate;

    out_stream.write(sum);
}