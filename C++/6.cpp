#include &lt;ap_fixed.h&gt; // If using fixed point, but use float for simplicity
#include &lt;hls_stream.h&gt;
#include &lt;cmath&gt; // For fmod or something

// Define constants
#define NUM_OSC 6
#define SAMPLE_RATE 44100.0f
#define PI 3.14159265f

// Simple PRNG for noise
unsigned int rand_state = 123456789;

float random() {
    rand_state = rand_state * 1103515245 + 12345;
    return (float)((rand_state / 65536) % 32768) / 32768.0f * 2.0f - 1.0f;
}

// LFNoise1 approximate
class LFNoise1 {
public:
    float curr;
    float target;
    float phase;
    float rate;
    float inc;

    LFNoise1(float r) : rate(r), phase(0.0f), curr(0.0f), target(0.0f), inc(0.0f) {}

    float process() {
        phase += rate / SAMPLE_RATE;
        if (phase >= 1.0f) {
            phase -= 1.0f;
            curr = target;
            target = random();
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

    Saw() : phase(0.0f), freq(440.0f) {}

    float process() {
        float out = phase * 2.0f - 1.0f;
        phase += freq / SAMPLE_RATE;
        if (phase > 1.0f) phase -= 1.0f;
        return out;
    }
};

// Simple reverb approximation, not full GVerb/FreeVerb
class SimpleReverb {
public:
    float buffer[10000]; // Fixed size
    int idx;
    float damp;
    float room;

    SimpleReverb() : idx(0), damp(0.6f), room(0.5f) {
        for (int i = 0; i < 10000; i++) buffer[i] = 0.0f;
    }

    float process(float in) {
        float out = buffer[idx];
        buffer[idx] = in + out * room;
        idx = (idx + 1) % (int)(50 * room * 100); // Approximate roomsize
        return out * (1.0f - damp) + in * 0.8f; // Dry mix
    }
};

// Top function for HLS
void ambient_drone(hls::stream&lt;float&gt; &outL, hls::stream&lt;float&gt; &outR, int num_samples) {
#pragma HLS INTERFACE axis port=outL
#pragma HLS INTERFACE axis port=outR
#pragma HLS INTERFACE s_axilite port=num_samples
#pragma HLS INTERFACE s_axilite port=return

    static LFNoise1 freq_noise(3.14f * 2);
    static LFNoise1 detune_noise[ NUM_OSC ];
    static Saw saws[ NUM_OSC ];
    static SimpleReverb reverb[ NUM_OSC ];

    for (int s = 0; s < num_samples; s++) {
#pragma HLS PIPELINE
        float sound = 0.0f;
        float freq_base = freq_noise.process() * (2000 - 30) / 2 + (30 + 2000)/2; // Range 30-2000

        for (int i = 0; i < NUM_OSC; i++) {
            if (detune_noise[i].rate == 0.0f) detune_noise[i].rate = 0.1f; // Init
            float detune = detune_noise[i].process() * 5.0f; // -5 to 5
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