% Parameters
sample_rate = 44100; % Standard audio sampling rate (Hz)
duration = 5.0; % Duration of the signal in seconds
num_samples = round(sample_rate * duration);
num_oscillators = 8; % Equivalent to Mix.fill(8, ...)
freq_choices = [0.5, 1, 2] * 3.14; % LFO frequencies: [1.57, 3.14, 6.28] Hz
base_freq = 47 + rand() * (48 - 47); % Random base frequency between 47 and 48 Hz
mod_depth = 1/8/4; % Modulation depth (0.03125)

% Time array
t = linspace(0, duration, num_samples)';

% Initialize output signal
mixed_signal = zeros(num_samples, 1);

% Generate 8 modulated sine oscillators
for i = 1:num_oscillators
    % Randomly choose LFO frequency
    lfo_freq = freq_choices(randi(length(freq_choices)));
    
    % Generate LFO signal for frequency modulation
    lfo = sin(2 * pi * lfo_freq * t);
    
    % Modulate the base frequency
    modulated_freq = base_freq + (mod_depth * lfo);
    
    % Generate sine wave with modulated frequency
    % Phase is cumulative sum of frequency to account for time-varying frequency
    phase = cumsum(2 * pi * modulated_freq / sample_rate);
    oscillator = sin(phase);
    
    % Add to mixed signal
    mixed_signal = mixed_signal + oscillator;
end

% Normalize the mixed signal
mixed_signal = mixed_signal / num_oscillators;

% Duplicate for stereo output (equivalent to !2)
stereo_signal = [mixed_signal, mixed_signal];

% Normalize to prevent clipping (scale to [-1, 1])
stereo_signal = stereo_signal / max(abs(stereo_signal(:)));

% Save to WAV file
audiowrite('output.wav', stereo_signal, sample_rate);

disp('Audio signal generated and saved as output.wav');