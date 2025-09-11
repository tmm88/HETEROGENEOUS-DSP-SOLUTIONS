#include <hls_math.h>
#include <ap_int.h>
#include <ap_fixed.h>

#define SR 44100.0
#define NUM_VOICES 5
#define MAX_GRAINS 512

// Define the grain struct
struct Grain {
    float counter;
    float dur_samples;
    float car_phase;
    float mod_phase;
    float car_inc;
    float mod_inc;
};

// Simple LFSR PRNG (replaces rand)
ap_uint<32> lfsr_next(ap_uint<32> state) {
    #pragma HLS INLINE
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Fold function
float sc_fold(float in, float lo, float hi) {
    #pragma HLS INLINE
    float x = in - lo;
    float range = hi - lo;
    float range2 = range + range;
    if (in >= hi) {
        in = hi + hi - in;
        if (in >= lo) return in;
    } else if (in < lo) {
        in = lo + lo - in;
        if (in < hi) return in;
    } else return in;
    if (range <= 0.f) return lo;
    x /= range;
    float c = x - (int)x;
    if (c < 0.f) c += 1.f;
    if (c > 0.5f) {
        return hi - range2 * (c - 0.5f);
    } else {
        return lo + range2 * c;
    }
}

// Main synthesis function
extern "C" {
void synth(float* out_buffer, int num_samples) {
    #pragma HLS INTERFACE m_axi port=out_buffer offset=slave bundle=gmem
    #pragma HLS INTERFACE s_axilite port=out_buffer bundle=control
    #pragma HLS INTERFACE s_axilite port=num_samples bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    float freqs[NUM_VOICES];
    float modFreqs[NUM_VOICES];

    // Generate pseudo-random freqs (fixed seed)
    ap_uint<32> rng_state = 1;
    for (int i = 0; i < NUM_VOICES; i++) {
        #pragma HLS UNROLL
        rng_state = lfsr_next(rng_state);
        freqs[i] = 100 + (rng_state % 5901);

        rng_state = lfsr_next(rng_state);
        modFreqs[i] = 100 + (rng_state % 5901);
    }

    float density = 100.0f;
    float scale = density / SR;
    float dust_counter = 1.0f;

    float line_level = 0.1f;
    float line_slope = (20.0f - 0.1f) / (5.0f * SR);
    float sin_phase = 0.0f;
    float sin_inc = 2.0f * M_PI * 20.0f / SR;

    Grain grains[NUM_VOICES][MAX_GRAINS];
    #pragma HLS ARRAY_PARTITION variable=grains dim=1 complete

    int num_active[NUM_VOICES] = {0};
    float prev_trig = 0.0f;

    for (int s = 0; s < num_samples; s++) {
        #pragma HLS PIPELINE II=1

        rng_state = lfsr_next(rng_state);
        float r = (float)rng_state / 0xFFFFFFFF;

        float trig = 0.0f;
        dust_counter -= 1.0f;

        if (dust_counter <= 0.0f) {
            float r_log = hls::logf(r);
            dust_counter = -r_log / scale;

            rng_state = lfsr_next(rng_state);
            float r2 = ((float)rng_state / 0xFFFFFFFF) * 2.0f - 1.0f;
            trig = r2;
        }

        if (trig > 0.0f && prev_trig <= 0.0f) {
            for (int v = 0; v < NUM_VOICES; v++) {
                if (num_active[v] < MAX_GRAINS) {
                    int g = num_active[v]++;
                    grains[v][g].dur_samples = 0.02f * SR;
                    grains[v][g].counter = grains[v][g].dur_samples;
                    grains[v][g].car_phase = 0.0f;
                    grains[v][g].mod_phase = 0.0f;
                    grains[v][g].car_inc = 2.0f * M_PI * freqs[v] / SR;
                    grains[v][g].mod_inc = 2.0f * M_PI * modFreqs[v] / SR;
                }
            }
        }

        prev_trig = trig;

        line_level += line_slope;
        if (line_level > 20.0f) line_level = 20.0f;

        float level = hls::sinf(sin_phase);
        sin_phase += sin_inc;
        if (sin_phase > 2.0f * M_PI) sin_phase -= 2.0f * M_PI;

        float mix = 0.0f;

        for (int v = 0; v < NUM_VOICES; v++) {
            float out = 0.0f;

            for (int g = 0; g < num_active[v]; g++) {
                Grain& gr = grains[v][g];
                if (gr.counter > 0) {
                    float mod = hls::sinf(gr.mod_phase);
                    float phase = gr.car_phase + mod * line_level;
                    float sig = hls::sinf(phase);

                    float fraction = 1.0f - (gr.counter / gr.dur_samples);
                    float env = 0.5f * (1.0f - hls::cosf(2.0f * M_PI * fraction));
                    out += sig * env;

                    gr.mod_phase += gr.mod_inc;
                    if (gr.mod_phase > 2.0f * M_PI) gr.mod_phase -= 2.0f * M_PI;

                    gr.car_phase += gr.car_inc;
                    if (gr.car_phase > 2.0f * M_PI) gr.car_phase -= 2.0f * M_PI;

                    gr.counter -= 1.0f;

                    if (gr.counter <= 0) {
                        grains[v][g] = grains[v][--num_active[v]];
                        g--;
                    }
                }
            }

            float lo = -hls::fabsf(level);
            float hi = hls::fabsf(level);
            float folded = sc_fold(out, lo, hi);
            folded *= 0.1f;
            mix += folded;
        }

        out_buffer[s * 2] = mix;
        out_buffer[s * 2 + 1] = mix;
    }
}
}
