#include <CL/cl.hpp>
#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <cmath>

// Sine table size and precompute
const int SINE_TABLE_SIZE = 1024;
std::vector<float> sine_table(SINE_TABLE_SIZE);
void init_sine_table() {
    for (int i = 0; i < SINE_TABLE_SIZE; ++i) {
        sine_table[i] = std::sin(2.0f * M_PI * i / SINE_TABLE_SIZE);
    }
}

int main() {
    // Parameters
    const int sample_rate = 44100;
    const float duration = 5.0f;
    const int num_samples = static_cast<int>(sample_rate * duration);
    const int num_oscillators = 8;
    const std::vector<float> freq_choices = {3.14f * 0.5f, 3.14f * 1.0f, 3.14f * 2.0f};
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(47.0f, 48.0f);
    const float base_freq = dist(gen);
    const float mod_depth = 1.0f / (8.0f * 4.0f);

    // Precompute LFO frequencies (random per oscillator)
    std::vector<float> lfo_freqs(num_oscillators);
    std::uniform_int_distribution<int> choice_dist(0, freq_choices.size() - 1);
    for (int i = 0; i < num_oscillators; ++i) {
        lfo_freqs[i] = freq_choices[choice_dist(gen)];
    }

    init_sine_table();

    // Get platform and device
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.empty()) {
        std::cerr << "No OpenCL platforms found." << std::endl;
        return 1;
    }
    cl::Platform platform = platforms[0];
    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR, &devices);  // Prefer GPU or DSP
    if (devices.empty()) {
        std::cerr << "No OpenCL devices found." << std::endl;
        return 1;
    }
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue queue(context, device);

    // Kernel source
    std::string kernel_source = R"(
        __kernel void fm_oscillator(
            __global float* output,
            __global const float* lfo_freqs,
            __global const float* sine_table,
            const float base_freq,
            const float mod_depth,
            const int sample_rate,
            const int num_samples,
            const int table_size,
            const int oscillator_id) {
            const float lfo_phase_inc = lfo_freqs[oscillator_id] / sample_rate * table_size;
            const uint phase_scale = (1U << 32) / table_size;  // For fixed-point phase
            uint phase_acc = 0;
            uint lfo_phase_acc = 0;
            for (int sample = 0; sample < num_samples; ++sample) {
                // LFO value
                int lfo_idx = (lfo_phase_acc >> (32 - 10)) % table_size;  // Assuming 10-bit for 1024
                float lfo = sine_table[lfo_idx];
                // Modulated frequency
                float mod_freq = base_freq + mod_depth * lfo;
                // Phase increment
                uint phase_inc = (uint)(mod_freq / sample_rate * table_size * phase_scale);
                // Accumulate phase and get sine
                phase_acc += phase_inc;
                int idx = (phase_acc >> (32 - 10)) % table_size;
                output[oscillator_id * num_samples + sample] = sine_table[idx];
                // Update LFO phase
                lfo_phase_acc += (uint)(lfo_phase_inc * phase_scale);
            }
        }
    )";

    // Build program
    cl::Program program(context, kernel_source);
    try {
        program.build({device});
    } catch (cl::Error& e) {
        std::cerr << "Build error: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
        return 1;
    }

    cl::Kernel kernel(program, "fm_oscillator");

    // Buffers
    std::vector<float> outputs(num_oscillators * num_samples, 0.0f);  // All oscillators' signals
    cl::Buffer output_buf(context, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * outputs.size(), outputs.data());
    cl::Buffer lfo_freqs_buf(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * lfo_freqs.size(), lfo_freqs.data());
    cl::Buffer sine_table_buf(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * sine_table.size(), sine_table.data());

    // Launch kernels (one per oscillator)
    for (int osc = 0; osc < num_oscillators; ++osc) {
        kernel.setArg(0, output_buf);
        kernel.setArg(1, lfo_freqs_buf);
        kernel.setArg(2, sine_table_buf);
        kernel.setArg(3, base_freq);
        kernel.setArg(4, mod_depth);
        kernel.setArg(5, sample_rate);
        kernel.setArg(6, num_samples);
        kernel.setArg(7, SINE_TABLE_SIZE);
        kernel.setArg(8, osc);

        queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(1), cl::NDRange(1));
    }
    queue.finish();

    // Read back
    queue.enqueueReadBuffer(output_buf, CL_TRUE, 0, sizeof(float) * outputs.size(), outputs.data());

    // Mix and normalize
    std::vector<float> mixed_signal(num_samples, 0.0f);
    for (int sample = 0; sample < num_samples; ++sample) {
        for (int osc = 0; osc < num_oscillators; ++osc) {
            mixed_signal[sample] += outputs[osc * num_samples + sample];
        }
        mixed_signal[sample] /= num_oscillators;
    }

    // Stereo output (duplicate)
    std::vector<int16_t> stereo_signal(num_samples * 2);
    float max_abs = 0.0f;
    for (float val : mixed_signal) {
        max_abs = std::max(max_abs, std::abs(val));
    }
    for (int i = 0; i < num_samples; ++i) {
        int16_t sample_val = static_cast<int16_t>((mixed_signal[i] / max_abs) * 32767);
        stereo_signal[2 * i] = sample_val;      // Left
        stereo_signal[2 * i + 1] = sample_val;  // Right
    }

    // Save WAV (simple header + data)
    std::ofstream wav_file("output.wav", std::ios::binary);
    if (!wav_file) {
        std::cerr << "Failed to open output.wav" << std::endl;
        return 1;
    }
    // WAV header (44 bytes)
    const char* header = "RIFF----WAVEfmt \x10\x00\x00\x00\x01\x00\x02\x00\x44\xAC\x00\x00\x10\xB1\x02\x00\x04\x00\x10\x00data----";
    wav_file.write(header, 44);
    // Update sizes
    int file_size = 36 + sizeof(int16_t) * stereo_signal.size();
    wav_file.seekp(4);
    wav_file.write(reinterpret_cast<const char*>(&file_size), 4);
    int data_size = sizeof(int16_t) * stereo_signal.size();
    wav_file.seekp(40);
    wav_file.write(reinterpret_cast<const char*>(&data_size), 4);
    // Write data
    wav_file.write(reinterpret_cast<const char*>(stereo_signal.data()), data_size);

    std::cout << "Audio signal generated and saved as 'output.wav'" << std::endl;
    return 0;
}
