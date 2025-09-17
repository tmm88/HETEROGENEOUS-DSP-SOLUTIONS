% MATLAB implementation of three SuperCollider FM synths
% Each function generates stereo audio samples based on input parameters

function [left, right] = fm_synth1(fs, duration)
    % Parameters
    sample_rate = fs; % e.g., 44100 Hz
    t = (0:1/sample_rate:duration)';
    N = length(t);
    
    % Initialize output
    left = zeros(N, 1);
    right = zeros(N, 1);
    
    % LFNoise approximation (smoothed random walk)
    modFreq_noise = zeros(N, 1);
    modIndex_noise = zeros(N, 1);
    cutoff_noise = zeros(N, 1);
    
    % Simple lowpass for LFNoise (approximating SuperCollider's LFNoise2)
    [b, a] = butter(1, 0.2/(sample_rate/2)); % 0.2 Hz cutoff
    modFreq_noise = filter(b, a, 2*rand(N, 1)-1);
    modFreq = 50 * (1 + 7 * (modFreq_noise - min(modFreq_noise)) / (max(modFreq_noise) - min(modFreq_noise))); % Range [50, 400]
    
    [b, a] = butter(1, 0.1/(sample_rate/2)); % 0.1 Hz cutoff
    modIndex_noise = filter(b, a, 2*rand(N, 1)-1);
    modIndex = 20 + 60 * (modIndex_noise - min(modIndex_noise)) / (max(modIndex_noise) - min(modIndex_noise)); % Range [20, 80]
    
    cutoff_noise = filter(b, a, 2*rand(N, 1)-1);
    cutoff = 300 + 1200 * (cutoff_noise - min(cutoff_noise)) / (max(cutoff_noise) - min(cutoff_noise)); % Range [300, 1500]
    
    % FM synthesis
    carriers = [60, 62, 90];
    drone = zeros(N, 1);
    
    for cfreq = carriers
        mod = modIndex .* sin(2 * pi * modFreq .* t);
        drone = drone + 0.1 * sin(2 * pi * (cfreq + mod) .* t);
    end
    
    % Sub oscillator
    sub = 0.1 * sin(2 * pi * 30 * t);
    
    % Combine
    sig = drone + sub;
    
    % RLPF (using variable cutoff)
    for n = 1:N
        [b, a] = butter(2, cutoff(n)/(sample_rate/2), 'low'); % 2nd order lowpass
        if n == 1
            sig(n) = sig(n); % No filtering for first sample
        else
            sig(n) = filter(b, a, sig(n), [], 1);
        end
    end
    
    % Tanh distortion
    sig = 0.3 * tanh(5 * sig);
    
    % FreeVerb approximation (simplified comb filter)
    mix = 0.4;
    roomsize = 0.6;
    damp = 0.3;
    delay_samples = round(0.02 * sample_rate); % Approx 20ms delay
    delay_line = zeros(delay_samples, 2); % Stereo
    reverb_out = zeros(N, 2);
    
    for n = 1:N
        % Comb filter for reverb
        for ch = 1:2
            delayed = delay_line(max(1, n - delay_samples + 1), ch);
            feedback = sig(n) + roomsize * delayed;
            delay_line(mod(n-1, delay_samples) + 1, ch) = (1 - damp) * feedback + damp * delayed;
            reverb_out(n, ch) = delayed;
        end
    end
    sig_stereo = (1 - mix) * sig + mix * reverb_out;
    
    % Splay (spread mono to stereo)
    left = sig_stereo(:, 1) * 0.5;
    right = sig_stereo(:, 2) * 0.5;
end

function [left, right] = fm_synth2(fs, duration)
    % Parameters
    sample_rate = fs;
    t = (0:1/sample_rate:duration)';
    N = length(t);
    
    % Initialize output
    left = zeros(N, 1);
    right = zeros(N, 1);
    
    % LFNoise
    [b, a] = butter(1, 0.2/(sample_rate/2));
    modFreq_noise = filter(b, a, 2*rand(N, 1)-1);
    modFreq = 50 * (1 + 5 * (modFreq_noise - min(modFreq_noise)) / (max(modFreq_noise) - min(modFreq_noise))); % Range [50, 300]
    
    [b, a] = butter(1, 0.1/(sample_rate/2));
    modIndex_noise = filter(b, a, 2*rand(N, 1)-1);
    modIndex = 10 + 50 * (modIndex_noise - min(modIndex_noise)) / (max(modIndex_noise) - min(modIndex_noise)); % Range [10, 60]
    
    cutoff_noise = filter(b, a, 2*rand(N, 1)-1);
    cutoff = 200 + 1000 * (cutoff_noise - min(cutoff_noise)) / (max(cutoff_noise) - min(cutoff_noise)); % Range [200, 1200]
    
    % FM synthesis
    carrier = 70;
    mod = modIndex .* sin(2 * pi * modFreq .* t);
    tone = 0.2 * sin(2 * pi * (carrier + mod) .* t);
    
    % Sub oscillator
    sub = 0.1 * sin(2 * pi * 30 * t);
    
    % Combine
    sig = tone + sub;
    
    % RLPF
    for n = 1:N
        [b, a] = butter(2, cutoff(n)/(sample_rate/2), 'low');
        if n == 1
            sig(n) = sig(n);
        else
            sig(n) = filter(b, a, sig(n), [], 1);
        end
    end
    
    % Tanh distortion
    sig = 0.3 * tanh(4 * sig);
    
    % FreeVerb
    mix = 0.3;
    roomsize = 0.6;
    damp = 0.3;
    delay_samples = round(0.02 * sample_rate);
    delay_line = zeros(delay_samples, 2);
    reverb_out = zeros(N, 2);
    
    for n = 1:N
        for ch = 1:2
            delayed = delay_line(max(1, n - delay_samples + 1), ch);
            feedback = sig(n) + roomsize * delayed;
            delay_line(mod(n-1, delay_samples) + 1, ch) = (1 - damp) * feedback + damp * delayed;
            reverb_out(n, ch) = delayed;
        end
    end
    sig_stereo = (1 - mix) * sig + mix * reverb_out;
    
    % Stereo output
    left = sig_stereo(:, 1);
    right = sig_stereo(:, 2);
end

function [left, right] = fm_synth3(fs, duration)
    % Parameters
    sample_rate = fs;
    t = (0:1/sample_rate:duration)';
    N = length(t);
    
    % Initialize output
    left = zeros(N, 1);
    right = zeros(N, 1);
    
    % FM synthesis
    mod = 50 * sin(2 * pi * 40 * t);
    tone = 0.2 * sin(2 * pi * (100 + mod) .* t);
    
    % RLPF
    [b, a] = butter(2, 800/(sample_rate/2), 'low');
    sig = filter(b, a, tone);
    
    % FreeVerb
    mix = 0.3;
    roomsize = 0.6;
    damp = 0.2;
    delay_samples = round(0.02 * sample_rate);
    delay_line = zeros(delay_samples, 2);
    reverb_out = zeros(N, 2);
    
    for n = 1:N
        for ch = 1:2
            delayed = delay_line(max(1, n - delay_samples + 1), ch);
            feedback = sig(n) + roomsize * delayed;
            delay_line(mod(n-1, delay_samples) + 1, ch) = (1 - damp) * feedback + damp * delayed;
            reverb_out(n, ch) = delayed;
        end
    end
    sig_stereo = (1 - mix) * sig + mix * reverb_out;
    
    % Stereo output
    left = sig_stereo(:, 1);
    right = sig_stereo(:, 2);
end

% Example usage
fs = 44100;
duration = 5; % seconds
[left1, right1] = fm_synth1(fs, duration);
[left2, right2] = fm_synth2(fs, duration);
[left3, right3] = fm_synth3(fs, duration);

% Combine or play individually
% sound([left1, right1], fs); % For synth1
% sound([left2, right2], fs); % For synth2
% sound([left3, right3], fs); % For synth3