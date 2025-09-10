#define NUM_INST 16
#define SR 44100.0f
#define REVERB_SIZE 22050

kernel void synth(global float* out, int num_samples, global float* phase, global float* env, global float* reverb_buffer, global int* reverb_idx, int offset) {
    int gid = get_global_id(0);
    if (gid >= num_samples) return;

    int sample_idx = offset + gid;
    float sum = 0.0f;

    int trigger = (sample_idx % (int)(SR / 10.0f)) == 0;  // Simplified trigger

    for (int i = 0; i < NUM_INST; i++) {
        float freq = 32.0f + i;

        if (trigger) env[i] = 0.0f;

        phase[i] += 2.0f * M_PI_F * freq / SR;
        if (phase[i] > 2.0f * M_PI_F) phase[i] -= 2.0f * M_PI_F;

        float osc = sin(phase[i]);

        if (env[i] < 1.0f) {
            env[i] += 100.0f / SR;  // Attack
        } else {
            env[i] -= 1.0f / SR;  // Release
            if (env[i] < 0.0f) env[i] = 0.0f;
        }

        float signal = osc * env[i];

        // Simple reverb (atomic for shared idx)
        int idx = atomic_inc(reverb_idx) % REVERB_SIZE;
        float rev = reverb_buffer[idx];
        reverb_buffer[idx] = signal + rev * 0.7f;
        sum += rev / NUM_INST;
    }

    out[gid] = sum;
}