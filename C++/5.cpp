#include <math.h>
#include <stdlib.h>

#define SR 44100.0
#define NUM_VOICES 5
#define MAX_GRAINS 512

struct Grain {
  double counter;
  double dur_samples;
  double car_phase;
  double mod_phase;
  double car_inc;
  double mod_inc;
};

double sc_fold(double in, double lo, double hi) {
  double x = in - lo;
  double range = hi - lo;
  double range2 = range + range;
  if (in >= hi) {
    in = hi + hi - in;
    if (in >= lo) return in;
  } else if (in < lo) {
    in = lo + lo - in;
    if (in < hi) return in;
  } else return in;
  if (range <= 0.) return lo;
  x /= range;
  double c = x - (long)x;
  if (c < 0.) c += 1.;
  if (c > .5) {
    return hi - range2 * (c - .5);
  } else {
    return lo + range2 * c;
  }
}

void synth(double *out_buffer, int num_samples) {
  #pragma HLS INTERFACE m_axi port=out_buffer offset=slave
  #pragma HLS INTERFACE s_axilite port=return bundle=CONTROL_BUS
  #pragma HLS INTERFACE s_axilite port=num_samples bundle=CONTROL_BUS
  double freqs[NUM_VOICES];
  double modFreqs[NUM_VOICES];
  for (int i=0; i<NUM_VOICES; i++) {
    freqs[i] = 100 + rand() % 5901;
    modFreqs[i] = 100 + rand() % 5901;
  }
  double density = 100.0;
  double scale = density / SR;
  double dust_counter = 1.0;
  double line_level = 0.1;
  double line_slope = (20.0 - 0.1) / (5.0 * SR);
  double sin_phase = 0.0;
  double sin_inc = 2 * M_PI * 20 / SR;
  Grain grains[NUM_VOICES][MAX_GRAINS];
  int num_active [NUM_VOICES] = {0};
  double prev_trig = 0.0;
  uint32_t rng_state = 1;
  for (int s=0; s<num_samples; s++) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    double r = (double)rng_state / 0xFFFFFFFF;
    double trig = 0.0;
    dust_counter--;
    if (dust_counter <= 0) {
      double r_log = log(r);
      dust_counter = -r_log / scale;
      rng_state ^= rng_state << 13;
      rng_state ^= rng_state >> 17;
      rng_state ^= rng_state << 5;
      double r2 = (double)rng_state / 0xFFFFFFFF * 2 - 1;
      trig = r2;
    }
    if (trig > 0.0 && prev_trig <= 0.0) {
      for (int v=0; v<NUM_VOICES; v++) {
        if (num_active[v] < MAX_GRAINS) {
          int g = num_active[v]++;
          grains[v][g].dur_samples = 0.02 * SR;
          grains[v][g].counter = grains[v][g].dur_samples;
          grains[v][g].car_phase = 0.0;
          grains[v][g].mod_phase = 0.0;
          grains[v][g].car_inc = 2 * M_PI * freqs[v] / SR;
          grains[v][g].mod_inc = 2 * M_PI * modFreqs[v] / SR;
        }
      }
    }
    prev_trig = trig;
    line_level += line_slope;
    if (line_level > 20.0) line_level = 20.0;
    double level = sin(sin_phase);
    sin_phase += sin_inc;
    if (sin_phase > 2 * M_PI) sin_phase -= 2 * M_PI;
    double mix = 0.0;
    for (int v=0; v<NUM_VOICES; v++) {
      double out = 0.0;
      for (int g=0; g<num_active[v]; g++) {
        Grain &gr = grains[v][g];
        if (gr.counter > 0) {
          double mod = sin(gr.mod_phase);
          double phase = gr.car_phase + mod * line_level;
          double sig = sin(phase);
          double fraction = 1.0 - (gr.counter / gr.dur_samples);
          double env = 0.5 * (1.0 - cos(2.0 * M_PI * fraction));
          out += sig * env;
          gr.mod_phase += gr.mod_inc;
          if (gr.mod_phase > 2 * M_PI) gr.mod_phase -= 2 * M_PI;
          gr.car_phase += gr.car_inc;
          if (gr.car_phase > 2 * M_PI) gr.car_phase -= 2 * M_PI;
          gr.counter -= 1.0;
          if (gr.counter <= 0) {
            grains[v][g] = grains[v][--num_active[v]];
            g--;
          }
        }
      }
      double lo = -fabs(level);
      double hi = fabs(level);
      double folded = sc_fold(out, lo, hi);
      folded *= 0.1;
      mix += folded;
    }
    out_buffer[s * 2] = mix;
    out_buffer[s * 2 + 1] = mix;
  }
}