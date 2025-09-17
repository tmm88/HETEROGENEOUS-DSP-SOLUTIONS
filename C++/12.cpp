#include <hls_stream.h>
#include <math.h>
#include <ap_int.h>  // For LFSR if needed, but using uint32_t

#define SAMPLE_RATE 44100.0f
#define PI 3.1415926535f
#define TABLE_SIZE 16384

// Assume sine_table is initialized in a separate init function or ROM
// For example:
// void init_sine_table(float sine_table[TABLE_SIZE]) {
//     for(int i = 0; i < TABLE_SIZE; i++) {
//         sine_table[i] = sinf(2 * PI * i / TABLE_SIZE);
//     }
// }
extern float sine_table[TABLE_SIZE];

inline float sin_lut(float phase) {
    int idx = (int)(fmodf(phase, 1.0f) * TABLE_SIZE);
    if (idx < 0) idx += TABLE_SIZE;
    return sine_table[idx];
}

// Simple LFSR for white noise
inline float get_white_noise() {
    static uint32_t lfsr = 0xACE1u;  // Non-zero seed
    uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 31);
    return 2.0f * (float)bit - 1.0f;
}

// Low-frequency noise approximation using 1-pole lowpass on white noise
float get_lfnoise(float rate) {
    static float noise_state1 = 0.0f;
    static float noise_state2 = 0.0f;  // Separate states for different rates

    float lp_coeff = expf(-2 * PI * rate / SAMPLE_RATE);
    float white = get_white_noise();

    // For LFNoise2 (use noise_state1), approximate
    float noise_out = lp_coeff * noise_state1 + (1 - lp_coeff) * white;
    noise_state1 = noise_out;

    return noise_out;
}

// Rational tanh approximation
inline float fast_tanh(float x) {
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Biquad filter for RLPF (lowpass resonant)
class BiquadLPF {
public:
    float a0, a1, a2, b1, b2;
    float z1, z2;
    float Fc, Q;

    BiquadLPF() : z1(0.0f), z2(0.0f), Fc(0.5f), Q(0.707f) {
        update_coeffs();
    }

    void setFcQ(float fc, float q) {
        Fc = fc / SAMPLE_RATE;
        Q = q;
        update_coeffs();
    }

    void update_coeffs() {
        float K = tanf(PI * Fc);
        float norm = 1.0f / (1.0f + K / Q + K * K);
        a0 = K * K * norm;
        a1 = 2.0f * a0;
        a2 = a0;
        b1 = 2.0f * (K * K - 1.0f) * norm;
        b2 = (1.0f - K / Q + K * K) * norm;
    }

    float process(float in) {
        float out = in * a0 + z1;
        z1 = in * a1 + z2 - b1 * out;
        z2 = in * a2 - b2 * out;
        return out;
    }
};

// Simple FreeVerb approximation (simplified to one comb per channel for brevity; full impl would use 8 combs + 4 allpasses)
class SimpleFreeVerb {
public:
    static const int DELAY_LEN = 1000;  // Approx for roomsize
    float delay_line[2][DELAY_LEN];  // Left and right
    int delay_ptr[2];
    float room_size, damp, mix;
    float comb_coeff, damp_coeff;

    SimpleFreeVerb() : room_size(0.6f), damp(0.3f), mix(0.4f) {
        comb_coeff = room_size;
        damp_coeff = 1.0f - damp;
        delay_ptr[0] = delay_ptr[1] = 0;
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < DELAY_LEN; ++i) {
                delay_line[ch][i] = 0.0f;
            }
        }
    }

    void setParams(float m, float r, float d) {
        mix = m;
        room_size = r;
        damp = d;
        comb_coeff = room_size;
        damp_coeff = 1.0f - damp;
    }

    void process(float& left, float& right) {
        // Simple mono to stereo comb reverb
        float input = (left + right) * 0.5f;

        // Left channel comb
        float delayed = delay_line[0][delay_ptr[0]];
        float filtered = damp_coeff * delayed + (1.0f - damp_coeff) * delay_line[0][(delay_ptr[0] + DELAY_LEN/2) % DELAY_LEN];
        float feedback = input + comb_coeff * filtered;
        delay_line[0][delay_ptr[0]] = feedback;
        delay_ptr[0] = (delay_ptr[0] + 1) % DELAY_LEN;
        float reverb_left = delayed;

        // Right channel (stereo spread)
        delayed = delay_line[1][delay_ptr[1]];
        filtered = damp_coeff * delayed + (1.0f - damp_coeff) * delay_line[1][(delay_ptr[1] + DELAY_LEN/2) % DELAY_LEN];
        feedback = input + comb_coeff * filtered;
        delay_line[1][delay_ptr[1]] = feedback;
        delay_ptr[1] = (delay_ptr[1] + 1) % DELAY_LEN;
        float reverb_right = delayed;

        left = left * (1.0f - mix) + reverb_left * mix;
        right = right * (1.0f - mix) + reverb_right * mix;
    }
};

// Top-level HLS function for first synth (processes one stereo sample per call)
void fm_synth1(hls::stream<float>& out_left, hls::stream<float>& out_right) {
#pragma HLS INTERFACE axis port=out_left
#pragma HLS INTERFACE axis port=out_right
#pragma HLS INTERFACE s_axilite port=return

    static float mod_phase = 0.0f;
    static float sub_phase = 0.0f;
    static float carrier_phases[3] = {0.0f, 0.0f, 0.0f};
    static BiquadLPF lpf;
    static SimpleFreeVerb reverb;

    // Get slow-varying parameters (update every sample for simplicity)
    float noise_modfreq = get_lfnoise(0.2f);
    float modFreq = ((noise_modfreq + 1.0f) / 2.0f * 7.0f + 1.0f) * 50.0f;

    float noise_modindex = get_lfnoise(0.1f);
    float modIndex = ((noise_modindex + 1.0f) / 2.0f * 60.0f + 20.0f);

    float noise_cutoff = get_lfnoise(0.1f);
    float cutoff = ((noise_cutoff + 1.0f) / 2.0f * 1200.0f + 300.0f);
    float rq = 0.3f;
    float Q = 1.0f / rq;
    lpf.setFcQ(cutoff, Q);

    // Modulator (shared)
    float mod_incr = modFreq / SAMPLE_RATE;
    float mod = sin_lut(mod_phase) * modIndex;
    mod_phase = fmodf(mod_phase + mod_incr, 1.0f);

    // Carriers
    float carriers[3] = {60.0f, 62.0f, 90.0f};
    float drone = 0.0f;
    for (int i = 0; i < 3; ++i) {
        float cfreq = carriers[i] + mod;
        float carrier_incr = cfreq / SAMPLE_RATE;
        float carrier = sin_lut(carrier_phases[i]) * 0.1f;
        drone += carrier;
        carrier_phases[i] = fmodf(carrier_phases[i] + carrier_incr, 1.0f);
    }

    // Sub oscillator
    float sub_incr = 30.0f / SAMPLE_RATE;
    float sub = sin_lut(sub_phase) * 0.1f;
    sub_phase = fmodf(sub_phase + sub_incr, 1.0f);

    float sig = drone + sub;

    // RLPF
    sig = lpf.process(sig);

    // Tanh distortion
    sig = fast_tanh(sig * 5.0f) * 0.3f;

    // FreeVerb (simplified)
    float left = sig;
    float right = sig;
    reverb.setParams(0.4f, 0.6f, 0.3f);  // Update params
    reverb.process(left, right);

    // Splay approx: for mono, just stereo copy with slight spread if needed
    // Here, simple stereo same
    out_left << left * 0.5f;  // Splay 0.5 approx
    out_right << right * 0.5f;
}

// Similar implementations for the other two synths...

// Second synth: single carrier
void fm_synth2(hls::stream<float>& out_left, hls::stream<float>& out_right) {
#pragma HLS INTERFACE axis port=out_left
#pragma HLS INTERFACE axis port=out_right
#pragma HLS INTERFACE s_axilite port=return

    static float mod_phase = 0.0f;
    static float carrier_phase = 0.0f;
    static float sub_phase = 0.0f;
    static BiquadLPF lpf;
    static SimpleFreeVerb reverb;

    float noise_modfreq = get_lfnoise(0.2f);
    float modFreq = ((noise_modfreq + 1.0f) / 2.0f * 5.0f + 1.0f) * 50.0f;  // range 1-6 *50

    float noise_modindex = get_lfnoise(0.1f);
    float modIndex = ((noise_modindex + 1.0f) / 2.0f * 50.0f + 10.0f);

    float noise_cutoff = get_lfnoise(0.1f);
    float cutoff = ((noise_cutoff + 1.0f) / 2.0f * 1000.0f + 200.0f);
    float rq = 0.3f;
    float Q = 1.0f / rq;
    lpf.setFcQ(cutoff, Q);

    float carrier_freq = 70.0f;

    float mod_incr = modFreq / SAMPLE_RATE;
    float mod = sin_lut(mod_phase) * modIndex;
    mod_phase = fmodf(mod_phase + mod_incr, 1.0f);

    float cfreq = carrier_freq + mod;
    float carrier_incr = cfreq / SAMPLE_RATE;
    float tone = sin_lut(carrier_phase) * 0.2f;
    carrier_phase = fmodf(carrier_phase + carrier_incr, 1.0f);

    float sub_incr = 30.0f / SAMPLE_RATE;
    float sub = sin_lut(sub_phase) * 0.1f;
    sub_phase = fmodf(sub_phase + sub_incr, 1.0f);

    float sig = tone + sub;

    sig = lpf.process(sig);

    sig = fast_tanh(sig * 4.0f) * 0.3f;

    float left = sig;
    float right = sig;
    reverb.setParams(0.3f, 0.6f, 0.3f);
    reverb.process(left, right);

    out_left << left;
    out_right << right;
}

// Third synth: simple fixed
void fm_synth3(hls::stream<float>& out_left, hls::stream<float>& out_right) {
#pragma HLS INTERFACE axis port=out_left
#pragma HLS INTERFACE axis port=out_right
#pragma HLS INTERFACE s_axilite port=return

    static float mod_phase = 0.0f;
    static float carrier_phase = 0.0f;
    static BiquadLPF lpf;  // Fixed cutoff 800
    static SimpleFreeVerb reverb;

    lpf.setFcQ(800.0f, 1.0f / 0.3f);  // rq=0.3

    float modFreq = 40.0f;
    float modIndex = 50.0f;
    float carrier_freq = 100.0f;

    float mod_incr = modFreq / SAMPLE_RATE;
    float mod = sin_lut(mod_phase) * modIndex;
    mod_phase = fmodf(mod_phase + mod_incr, 1.0f);

    float cfreq = carrier_freq + mod;
    float carrier_incr = cfreq / SAMPLE_RATE;
    float tone = sin_lut(carrier_phase) * 0.2f;
    carrier_phase = fmodf(carrier_phase + carrier_incr, 1.0f);

    float sig = lpf.process(tone);

    sig = reverb.process(sig, sig);  // Simplified, params fixed 0.3,0.6,0.2

    reverb.setParams(0.3f, 0.6f, 0.2f);

    out_left << sig;
    out_right << sig;
}
