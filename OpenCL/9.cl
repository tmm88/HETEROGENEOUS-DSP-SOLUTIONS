// OpenCL kernel for audio synthesis
// Simulates SuperCollider's Mix.fill(8, {SinOsc.ar(rrand(20,200),0,SinOsc.ar([1,2,4,8].choose*0.01,0,1/8/4,0.01))})
__kernel void audio_synth(__global float* output, uint sample_rate, float time)
{
    // Get global ID to index into output buffer
    uint gid = get_global_id(0);
    
    // Constants
    const float TWO_PI = 6.28318530718f;
    const int NUM_OSC = 8; // Number of oscillators to mix
    const float AMP_SCALE = 1.0f / 8.0f / 4.0f; // 1/8/4 for amplitude scaling
    const float AMP_OFFSET = 0.01f; // Offset for amplitude modulation
    
    // Simple pseudo-random number generator (since OpenCL lacks rand())
    uint seed = gid + (uint)(time * 1000.0f);
    float rand_float = (float)((seed * 1103515245u + 12345u) & 0x7fffffff) / (float)0x7fffffff;
    
    // Generate random frequency between 20 and 200 Hz
    float base_freq = 20.0f + rand_float * (200.0f - 20.0f);
    
    // Select modulation frequency from [1, 2, 4, 8] Hz
    float mod_freqs[4] = {1.0f, 2.0f, 4.0f, 8.0f};
    int mod_idx = (int)(rand_float * 4.0f) % 4;
    float mod_freq = mod_freqs[mod_idx] * 0.01f;
    
    // Compute time for this sample
    float sample_time = time + (float)gid / (float)sample_rate;
    
    // Compute amplitude modulation: SinOsc.ar(mod_freq, 0, AMP_SCALE, AMP_OFFSET)
    float amp = sin(TWO_PI * mod_freq * sample_time) * AMP_SCALE + AMP_OFFSET;
    
    // Compute main oscillator: SinOsc.ar(base_freq, 0, amp)
    float sample = sin(TWO_PI * base_freq * sample_time) * amp;
    
    // Mix by accumulating samples (simplified, assuming parallel reduction or single-thread mixing)
    if (gid < NUM_OSC) {
        // Store sample in output buffer (to be summed later)
        output[gid] = sample;
    }
}