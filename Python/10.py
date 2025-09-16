import cusignal
import cupy as cp
import numpy as np
from scipy.io import wavfile
import random

# Parameters
sample_rate = 44100  # Standard audio sampling rate (Hz)
duration = 5.0  # Duration of the signal in seconds
num_samples = int(sample_rate * duration)
num_oscillators = 8  # Equivalent to Mix.fill(8, ...)
freq_choices = [3.14 * x for x in [0.5, 1, 2]]  # Frequencies for LFO: [1.57, 3.14, 6.28] Hz
base_freq = random.uniform(47, 48)  # Random base frequency between 47 and 48 Hz
mod_depth = 1/8/4  # Modulation depth (0.03125)

# Time array (on GPU)
t = cp.linspace(0, duration, num_samples, endpoint=False)

# Initialize output signal
mixed_signal = cp.zeros(num_samples)

# Generate 8 modulated sine oscillators
for _ in range(num_oscillators):
    # Randomly choose LFO frequency
    lfo_freq = random.choice(freq_choices)
    
    # Generate LFO signal for frequency modulation (sin(2π * lfo_freq * t))
    lfo = cp.sin(2 * cp.pi * lfo_freq * t)
    
    # Modulate the base frequency
    modulated_freq = base_freq + (mod_depth * lfo)
    
    # Generate sine wave with modulated frequency
    # Phase is cumulative sum of frequency to account for time-varying frequency
    phase = cp.cumsum(2 * cp.pi * modulated_freq / sample_rate)
    oscillator = cp.sin(phase)
    
    # Add to mixed signal
    mixed_signal += oscillator

# Normalize the mixed signal
mixed_signal /= num_oscillators

# Duplicate for stereo output (equivalent to !2)
stereo_signal = cp.vstack((mixed_signal, mixed_signal)).T  # Shape: (num_samples, 2)

# Move to CPU for saving as WAV
stereo_signal_cpu = cp.asnumpy(stereo_signal)

# Normalize to 16-bit PCM range
stereo_signal_cpu = np.int16(stereo_signal_cpu / cp.max(cp.abs(stereo_signal)) * 32767)

# Save to WAV file
wavfile.write("output.wav", sample_rate, stereo_signal_cpu)

print("Audio signal generated and saved as 'output.wav'")