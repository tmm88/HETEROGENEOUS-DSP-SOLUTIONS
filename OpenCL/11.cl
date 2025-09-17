// OpenCL kernel implementation of three SuperCollider FM synths
// Each kernel processes one stereo sample at a time

#define SAMPLE_RATE 44100.0f
#define PI 3.1415926535f
#define TABLE_SIZE 16384

// Sine lookup table (assumed preloaded into global memory)
__constant float sine_table[TABLE_SIZE];

// Simple LFSR for white noise
float get_white_noise(uint* lfsr) {
    uint bit = ((*lfsr >> 0) ^ (*lfsr >> 2) ^ (*lfsr >> 3) ^ (*lfsr >> 5)) & 1u;
    *lfsr = (*lfsr >> 1) | (bit << 31);
    return 2.0f * (float)bit - 1.0f;
}

// Low-frequency noise using 1-pole lowpass
float get_lfnoise(float rate, float* state, uint* lfsr) {
    float lp_coeff = exp(-2.0f * PI * rate / SAMPLE_RATE);
    float white = get_white_noise(lfsr);
    float noise_out = lp_coeff * (*state) + (1.0f - lp_coeff) * white;
    *state = noise_out;
    return noise_out;
}

// Fast tanh approximation
float fast_tanh(float x) {
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Sine lookup
float sin_lut(float phase) {
    int idx = (int)(fmod(phase, 1.0f) * TABLE_SIZE);
    if (idx < 0) idx += TABLE_SIZE;
    return sine_table[idx];
}

// Biquad lowpass filter
typedef struct {
    float a0, a1, a2, b1, b2;
    float z1, z2;
} BiquadLPF;

void update_biquad_coeffs(BiquadLPF* lpf, float fc, float q) {
    float K = tan(PI * fc / SAMPLE_RATE);
    float norm = 1.0f / (1.0f + K / q + K * K);
    lpf->a0 = K * K * norm;
    lpf->a1 = 2.0f * lpf->a0;
    lpf->a2 = lpf->a0;
    lpf->b1 = 2.0f * (K * K - 1.0f) * norm;
    lpf->b2 = (1.0f - K / q + K * K) * norm;
}

float biquad_process(BiquadLPF* lpf, float in) {
    float out = in * lpf->a0 + lpf->z1;
    lpf->z1 = in * lpf->a1 + lpf->z2 - lpf->b1 * out;
    lpf->z2 = in * lpf->a2 - lpf->b2 * out;
    return out;
}

// Simplified FreeVerb (one comb per channel)
typedef struct {
    float delay_line[2][1000]; // Stereo, ~20ms at 44.1kHz
    int delay_ptr[2];
    float room_size, damp, mix;
} SimpleFreeVerb;

void reverb_process(SimpleFreeVerb* reverb, float* left, float* right) {
    float input = (*left + *right) * 0.5f;
    float delayed, filtered, feedback;

    // Left channel
    delayed = reverb->delay_line[0][reverb->delay_ptr[0]];
    filtered = (1.0f - reverb->damp) * delayed + reverb->damp * reverb->delay_line[0][(reverb->delay_ptr[0] + 500) % 1000];
    feedback = input + reverb->room_size * filtered;
    reverb->delay_line[0][reverb->delay_ptr[0]] = feedback;
    reverb->delay_ptr[0] = (reverb->delay_ptr[0] + 1) % 1000;
    *left = *left * (1.0f - reverb->mix) + delayed * reverb->mix;

    // Right channel
    delayed = reverb->delay_line[1][reverb->delay_ptr[1]];
    filtered = (1.0f - reverb->damp) * delayed + reverb->damp * reverb->delay_line[1][(reverb->delay_ptr[1] + 500) % 1000];
    feedback = input + reverb->room_size * filtered;
    reverb->delay_line[1][reverb->delay_ptr[1]] = feedback;
    reverb->delay_ptr[1] = (reverb->delay_ptr[1] + 1) % 1000;
    *right = *right * (1.0f - reverb->mix) + delayed * reverb->mix;
}

// Kernel for first synth
__kernel void fm_synth1(__global float* out_left, __global float* out_right, __global uint* sample_idx, __global float* state_mem) {
    size_t id = get_global_id(0);
    uint idx = *sample_idx + id;

    // Persistent state
    __global float* mod_phase = &state_mem[0];
    __global float* sub_phase = &state_mem[1];
    __global float* carrier_phases = &state_mem[2]; // [2,3,4]
    __global float* noise_state_modfreq = &state_mem[5];
    __global float* noise_state_modindex = &state_mem[6];
    __global float* noise_state_cutoff = &state_mem[7];
    __global uint* lfsr = (__global uint*)&state_mem[8];
    __global BiquadLPF* lpf = (__global BiquadLPF*)&state_mem[9];
    __global SimpleFreeVerb* reverb = (__global SimpleFreeVerb*)&state_mem[10];

    // LFNoise
    float modFreq = 50.0f * (1.0f + 7.0f * (get_lfnoise(0.2f, noise_state_modfreq, lfsr) + 1.0f) / 2.0f); // [50, 400]
    float modIndex = 20.0f + 60.0f * (get_lfnoise(0.1f, noise_state_modindex, lfsr) + 1.0f) / 2.0f; // [20, 80]
    float cutoff = 300.0f + 1200.0f * (get_lfnoise(0.1f, noise_state_cutoff, lfsr) + 1.0f) / 2.0f; // [300, 1500]

    // Update filter
    update_biquad_coeffs(lpf, cutoff, 1.0f / 0.3f);

    // Modulator
    float mod_incr = modFreq / SAMPLE_RATE;
    float mod = sin_lut(*mod_phase) * modIndex;
    *mod_phase = fmod(*mod_phase + mod_incr, 1.0f);

    // Carriers
    float carriers[3] = {60.0f, 62.0f, 90.0f};
    float drone = 0.0f;
    for (int i = 0; i < 3; i++) {
        float cfreq = carriers[i] + mod;
        float carrier_incr = cfreq / SAMPLE_RATE;
        float carrier = sin_lut(carrier_phases[i]) * 0.1f;
        drone += carrier;
        carrier_phases[i] = fmod(carrier_phases[i] + carrier_incr, 1.0f);
    }

    // Sub oscillator
    float sub_incr = 30.0f / SAMPLE_RATE;
    float sub = sin_lut(*sub_phase) * 0.1f;
    *sub_phase = fmod(*sub_phase + sub_incr, 1.0f);

    // Combine
    float sig = drone + sub;

    // RLPF
    sig = biquad_process(lpf, sig);

    // Tanh distortion
    sig = fast_tanh(sig * 5.0f) * 0.3f;

    // FreeVerb
    reverb->mix = 0.4f;
    reverb->room_size = 0.6f;
    reverb->damp = 0.3f;
    float left = sig;
    float right = sig;
    reverb_process(reverb, &left, &right);

    // Splay
    out_left[id] = left * 0.5f;
    out_right[id] = right * 0.5f;
}

// Kernel for second synth
__kernel void fm_synth2(__global float* out_left, __global float* out_right, __global uint* sample_idx, __global float* state_mem) {
    size_t id = get_global_id(0);
    uint idx = *sample_idx + id;

    // Persistent state
    __global float* mod_phase = &state_mem[0];
    __global float* carrier_phase = &state_mem[1];
    __global float* sub_phase = &state_mem[2];
    __global float* noise_state_modfreq = &state_mem[3];
    __global float* noise_state_modindex = &state_mem[4];
    __global float* noise_state_cutoff = &state_mem[5];
    __global uint* lfsr = (__global uint*)&state_mem[6];
    __global BiquadLPF* lpf = (__global BiquadLPF*)&state_mem[7];
    __global SimpleFreeVerb* reverb = (__global SimpleFreeVerb*)&state_mem[8];

    // LFNoise
    float modFreq = 50.0f * (1.0f + 5.0f * (get_lfnoise(0.2f, noise_state_modfreq, lfsr) + 1.0f) / 2.0f); // [50, 300]
    float modIndex = 10.0f + 50.0f * (get_lfnoise(0.1f, noise_state_modindex, lfsr) + 1.0f) / 2.0f; // [10, 60]
    float cutoff = 200.0f + 1000.0f * (get_lfnoise(0.1f, noise_state_cutoff, lfsr) + 1.0f) / 2.0f; // [200, 1200]

    // Update filter
    update_biquad_coeffs(lpf, cutoff, 1.0f / 0.3f);

    // Modulator
    float mod_incr = modFreq / SAMPLE_RATE;
    float mod = sin_lut(*mod_phase) * modIndex;
    *mod_phase = fmod(*mod_phase + mod_incr, 1.0f);

    // Carrier
    float carrier = 70.0f;
    float cfreq = carrier + mod;
    float carrier_incr = cfreq / SAMPLE_RATE;
    float tone = sin_lut(*carrier_phase) * 0.2f;
    *carrier_phase = fmod(*carrier_phase + carrier_incr, 1.0f);

    // Sub oscillator
    float sub_incr = 30.0f / SAMPLE_RATE;
    float sub = sin_lut(*sub_phase) * 0.1f;
    *sub_phase = fmod(*sub_phase + sub_incr, 1.0f);

    // Combine
    float sig = tone + sub;

    // RLPF
    sig = biquad_process(lpf, sig);

    // Tanh distortion
    sig = fast_tanh(sig * 4.0f) * 0.3f;

    // FreeVerb
    reverb->mix = 0.3f;
    reverb->room_size = 0.6f;
    reverb->damp = 0.3f;
    float left = sig;
    float right = sig;
    reverb_process(reverb, &left, &right);

    out_left[id] = left;
    out_right[id] = right;
}

// Kernel for third synth
__kernel void fm_synth3(__global float* out_left, __global float* out_right, __global uint* sample_idx, __global float* state_mem) {
    size_t id = get_global_id(0);
    uint idx = *sample_idx + id;

    // Persistent state
    __global float* mod_phase = &state_mem[0];
    __global float* carrier_phase = &state_mem[1];
    __global BiquadLPF* lpf = (__global BiquadLPF*)&state_mem[2];
    __global SimpleFreeVerb* reverb = (__global SimpleFreeVerb*)&state_mem[3];

    // Fixed filter
    update_biquad_coeffs(lpf, 800.0f, 1.0f / 0.3f);

    // Modulator
    float modFreq = 40.0f;
    float modIndex = 50.0f;
    float mod_incr = modFreq / SAMPLE_RATE;
    float mod = sin_lut(*mod_phase) * modIndex;
    *mod_phase = fmod(*mod_phase + mod_incr, 1.0f);

    // Carrier
    float carrier = 100.0f;
    float cfreq = carrier + mod;
    float carrier_incr = cfreq / SAMPLE_RATE;
    float tone = sin_lut(*carrier_phase) * 0.2f;
    *carrier_phase = fmod(*carrier_phase + carrier_incr, 1.0f);

    // RLPF
    float sig = biquad_process(lpf, tone);

    // FreeVerb
    reverb->mix = 0.3f;
    reverb->room_size = 0.6f;
    reverb->damp = 0.2f;
    float left = sig;
    float right = sig;
    reverb_process(reverb, &left, &right);

    out_left[id] = left;
    out_right[id] = right;
}