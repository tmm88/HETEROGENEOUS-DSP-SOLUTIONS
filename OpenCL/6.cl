__kernel void ambient_drone(__global float *outputL, __global float *outputR, int num_samples, unsigned int seed) {
    int gid = get_global_id(0);
    if (gid >= num_samples) return;

    // Per sample computation
    // Simple random for noise
    unsigned int rnd = seed + gid * 12345;
    rnd = rnd * 1103515245 + 12345;

    float freq = (rnd % 1970) + 30.0f;  // Approximate

    float sound = 0.0f;
    for (int i = 0; i < 6; i++) {
        rnd = rnd * 1103515245 + 12345;
        float detune = ((rnd % 10) - 5.0f);
        float phase = fmod((float)gid * (freq + detune) / 44100.0f, 1.0f);
        float saw = phase * 2.0f - 1.0f;

        // Simple reverb approx: echo
        float rev = saw + 0.5f * sin(phase * 2.0f * 3.1415f);  // Fake

        sound += rev * 0.1f;
    }
    sound *= 0.6f;

    outputL[gid] = sound;
    outputR[gid] = sound;
}

// To use: enqueue kernel with global size num_samples