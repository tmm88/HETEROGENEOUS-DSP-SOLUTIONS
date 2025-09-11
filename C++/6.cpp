#include <ap_fixed.h>
#include <hls_stream.h>
#include <cmath>

// Define constants
#define NUM_OSC 6
#define SAMPLE_RATE 44100.0f
#define PI 3.14159265f

// Simple PRNG for noise
static unsigned int rand_state = 123456789;

float random_float() {
    rand_state = rand_state * 1103515245 + 12345;
    return (float)((rand_state / 65536) % 32768) / 32768.0f * 2.0f - 1.0f;
}

// LFNoise1 class for LFO
class LFNoise1 {
public:
    float curr;
    float target;
    float phase;
    float rate;
    float inc;

    LFNoise1(float r = 0.0f) : rate(r), phase(0.0f), curr(0.0f), target(0.0f), inc(0.0f) {
        // Initial state
        target = random_float();
        inc = (target - curr);
    }

    float process() {
        phase += rate / SAMPLE_RATE;
        if (phase >= 1.0f) {
            phase -= 1.0f;
            curr = target;
            target = random_float();
            inc = (target - curr);
        }
        return curr + inc * phase;
    }
};

// Saw oscillator
class Saw {
public:
    float phase;
    float freq;

    Saw(float initial_freq = 440.0f) : phase(0.0f), freq(initial_freq) {}

    float process() {
        float out = phase * 2.0f - 1.0f;
        phase += freq / SAMPLE_RATE;
        if (phase > 1.0f) phase -= 1.0f;
        return out;
    }
};

// Simple reverb approximation
template<int BUFFER_SIZE>
class SimpleReverb {
public:
    float buffer[BUFFER_SIZE];
    int idx;
    float damp;
    float room;

    SimpleReverb(float d = 0.6f, float r = 0.5f) : idx(0), damp(d), room(r) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
#pragma HLS UNROLL
            buffer[i] = 0.0f;
        }
    }

    float process(float in) {
        float out = buffer[idx];
        buffer[idx] = in + out * room;
        idx = (idx + 1) % BUFFER_SIZE;
        return out * (1.0f - damp) + in * 0.8f;
    }
};

// Top function for HLS
void ambient_drone(hls::stream<float> &outL, hls::stream<float> &outR, int num_samples) {
#pragma HLS INTERFACE axis port=outL
#pragma HLS INTERFACE axis port=outR
#pragma HLS INTERFACE s_axilite port=num_samples
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    // Static declarations for state preservation
    static LFNoise1 freq_noise(PI * 2);
    static LFNoise1 detune_noise[NUM_OSC];
    static Saw saws[NUM_OSC];
    static SimpleReverb<10000> reverb[NUM_OSC];

    // Initialize detune rates
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < NUM_OSC; i++) {
            detune_noise[i].rate = 0.1f;
        }
        initialized = true;
    }

    for (int s = 0; s < num_samples; s++) {
#pragma HLS PIPELINE II=1
        float sound = 0.0f;
        float freq_base = freq_noise.process() * (2000 - 30) / 2 + (30 + 2000) / 2;

        for (int i = 0; i < NUM_OSC; i++) {
#pragma HLS UNROLL
            float detune = detune_noise[i].process() * 5.0f;
            saws[i].freq = freq_base + detune;
            float osc = saws[i].process();
            float rev = reverb[i].process(osc) * 0.1f;
            sound += rev;
        }
        sound *= 0.6f;

        outL.write(sound);
        outR.write(sound);
    }
}