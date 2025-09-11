float sc_fold(float in, float lo, float hi) {
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
  float c = x - floor(x);
  if (c < 0.f) c += 1.f;
  if (c > 0.5f) {
    return hi - range2 * (c - 0.5f);
  } else {
    return lo + range2 * c;
  }
}

kernel void synth(global float *out_buffer, int num_samples) {
  float SR = 44100.0f;
  int NUM_VOICES = 5;
  int MAX_GRAINS = 512;
  struct Grain {
    float counter;
    float dur_samples;
    float car_phase;
    float mod_phase;
    float car_inc;
    float mod_inc;
  };
  struct Grain grains[5][512];
  int num_active[5] = {0};
  float freqs[5] = {100.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f};
  float modFreqs[5] = {200.0f, 1200.0f, 2200.0f, 3200.0f, 4200.0f};
  float density = 100.0f;
  float scale = density / SR;
  float dust_counter = 1.0f;
  float line_level = 0.1f;
  float line_slope = (20.0f - 0.1f) / (5.0f * SR);
  float sin_phase = 0.0f;
  float sin_inc = 2.0f * M_PI_F * 20.0f / SR;
  float prev_trig = 0.0f;
  uint rng_state = 1;
  for (int s = 0; s < num_samples; s++) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    float r = (float) rng_state / 0xFFFFFFFFU;
    float trig = 0.0f;
    dust_counter--;
    if (dust_counter <= 0.0f) {
      float r_log = log(r);
      dust_counter = -r_log / scale;
      rng_state ^= rng_state << 13;
      rng_state ^= rng_state >> 17;
      rng_state ^= rng_state << 5;
      float r2 = (float) rng_state / 0xFFFFFFFFU * 2.0f - 1.0f;
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
          grains[v][g].car_inc = 2.0f * M_PI_F * freqs[v] / SR;
          grains[v][g].mod_inc = 2.0f * M_PI_F * modFreqs[v] / SR;
        }
      }
    }
    prev_trig = trig;
    line_level += line_slope;
    if (line_level > 20.0f) line_level = 20.0f;
    float level = sin(sin_phase);
    sin_phase += sin_inc;
    float mix = 0.0f;
    for (int v = 0; v < NUM_VOICES; v++) {
      float out = 0.0f;
      for (int g = 0; g < num_active[v]; g++) {
        struct Grain *gr = &grains[v][g];
        if (gr->counter > 0.0f) {
          float mod = sin(gr->mod_phase);
          float phase = gr->car_phase + mod * line_level;
          float sig = sin(phase);
          float fraction = 1.0f - (gr->counter / gr->dur_samples);
          float env = 0.5f * (1.0f - cos(2.0f * M_PI_F * fraction));
          out += sig * env;
          gr->mod_phase += gr->mod_inc;
          gr->car_phase += gr->car_inc;
          gr->counter -= 1.0f;
          if (gr->counter <= 0.0f) {
            grains[v][g] = grains[v][--num_active[v]];
            g--;
          }
        }
      }
      float lo = -fabs(level);
      float hi = fabs(level);
      float folded = sc_fold(out, lo, hi);
      folded *= 0.1f;
      mix += folded;
    }
    out_buffer[s * 2] = mix;
    out_buffer[s * 2 + 1] = mix;
  }
}