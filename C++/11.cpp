// C++ HLS code for audio synthesis on Alinx FPGA
// Implements approximation of SuperCollider: Mix.fill(8, {SinOsc.ar(rrand(20,200),0,SinOsc.ar([1,2,4,8].choose*0.01,0,1/8/4,0.01))})
// Uses wavetable lookup for sine waves
// Frequencies are hardcoded to simulate random
// Assumes external memory for wavetable, initialized by host (e.g., ARM on Zynq)
// Output is stereo via hls_stream, to be connected to I2S in top-level HDL
// Sample rate assumed 44100 Hz

#include <hls_stream.h>
#include <ap_int.h>
#include <hls_math.h>

#define NUM_OSC 8
#define TABLE_SIZE 16384
#define SAMPLE_RATE 44100.0f

// Top-level function
void audio_synth(
    hls::stream<ap_int<24>>& audio_left,
    hls::stream<ap_int<24>>& audio_right,
    float* wavetable,  // m_axi to DDR
    ap_uint<1> arm_ok  // Control signal
) {
#pragma HLS INTERFACE m_axi port=wavetable offset=slave bundle=gmem latency=30
#pragma HLS INTERFACE axis port=audio_left
#pragma HLS INTERFACE axis port=audio_right
#pragma HLS INTERFACE ap_ctrl_hs port=return
#pragma HLS INTERFACE ap_none port=arm_ok

    static bool initialized = false;
    static float phase_main[NUM_OSC];
    static float phase_mod[NUM_OSC];

    const float base_freq[NUM_OSC] = {30.0f, 55.0f, 80.0f, 110.0f, 140.0f, 165.0f, 185.0f, 195.0f};
    const float mod_freq[NUM_OSC] = {0.01f, 0.04f, 0.02f, 0.08f, 0.01f, 0.02f, 0.04f, 0.08f};
    const float amp_scale = 1.0f / 8.0f / 4.0f;
    const float amp_offset = 0.01f;

    if (arm_ok) {
        if (!initialized) {
            // Initialize phases
            for (int i = 0; i < NUM_OSC; ++i) {
                phase_main[i] = 0.0f;
                phase_mod[i] = 0.0f;
            }
            initialized = true;
        } else {
            // Compute one sample
            float sum = 0.0f;

            loop_osc: for (int i = 0; i < NUM_OSC; ++i) {
#pragma HLS UNROLL
                // Main oscillator
                float incr_main = base_freq[i] / SAMPLE_RATE;
                phase_main[i] += incr_main;
                if (phase_main[i] >= 1.0f) phase_main[i] -= 1.0f;  // Instead of fmodf for efficiency
                int idx_main = (int)(phase_main[i] * TABLE_SIZE);
                float osc = wavetable[idx_main];

                // Mod oscillator for amplitude
                float incr_mod = mod_freq[i] / SAMPLE_RATE;
                phase_mod[i] += incr_mod;
                if (phase_mod[i] >= 1.0f) phase_mod[i] -= 1.0f;
                int idx_mod = (int)(phase_mod[i] * TABLE_SIZE);
                float mod_amp = wavetable[idx_mod] * amp_scale + amp_offset;

                // Accumulate
                sum += osc * mod_amp;
            }

            // Scale to 24-bit signed integer (-2^23 to 2^23-1)
            ap_int<24> out_sample = (ap_int<24>)(sum * 8388607.0f);  // 2^23 - 1 ≈ 8388608

            // Write to streams
            audio_left.write(out_sample);
            audio_right.write(out_sample);
        }
    }