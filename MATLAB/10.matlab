% MATLAB implementation of four SuperCollider radio signal examples
% Generates and plays four different modulated sine wave mixtures

clear all;
close all;

% Audio parameters
fs = 44100;          % Sampling rate (Hz)
duration = 2;        % Duration of each signal (seconds)
t = 0:1/fs:duration-1/fs; % Time vector
numOsc = 8;          % Number of oscillators to mix

% Signal 1: Simple mix of 8 sine waves with random frequencies
fprintf('Playing Signal 1: Simple mix of sine waves\n');
sig1 = zeros(1, length(t));
for i = 1:numOsc
    freq = randi([20, 200]); % Random frequency between 20 and 200 Hz
    sig1 = sig1 + (1/8/4) * sin(2 * pi * freq * t); % Sum with amplitude 1/8/4
end
soundsc(sig1, fs); % Play the sound
pause(duration + 0.5); % Pause to separate signals

% Signal 2: Frequency modulation with random modulator frequencies
fprintf('Playing Signal 2: Frequency modulated sine waves\n');
sig2 = zeros(1, length(t));
modFreqs = [1, 2, 4, 8]; % Possible modulator frequencies
for i = 1:numOsc
    baseFreq = randi([20, 200]);
    modFreq = modFreqs(randi([1, length(modFreqs)]));
    % Frequency modulation: sin(2π * (baseFreq + sin(2π * modFreq * t)) * t)
    sig2 = sig2 + (1/8/4) * sin(2 * pi * baseFreq * (1 + sin(2 * pi * modFreq * t)) .* t);
end
soundsc(sig2, fs);
pause(duration + 0.5);

% Signal 3: Both frequency and amplitude modulation
fprintf('Playing Signal 3: Frequency and amplitude modulated sine waves\n');
sig3 = zeros(1, length(t));
for i = 1:numOsc
    baseFreq = randi([20, 200]);
    modFreq = modFreqs(randi([1, length(modFreqs)]));
    ampModFreq = modFreqs(randi([1, length(modFreqs)])) * 0.01;
    % Amplitude modulation with offset: (0.01 + sin(2π * ampModFreq * t)) * 1/8/4
    amp = (0.01 + sin(2 * pi * ampModFreq * t)) * (1/8/4);
    % Frequency modulation
    sig3 = sig3 + amp .* sin(2 * pi * baseFreq * (1 + sin(2 * pi * modFreq * t)) .* t);
end
soundsc(sig3, fs);
pause(duration + 0.5);

% Signal 4: Amplitude modulation only
fprintf('Playing Signal 4: Amplitude modulated sine waves\n');
sig4 = zeros(1, length(t));
for i = 1:numOsc
    freq = randi([20, 200]);
    ampModFreq = modFreqs(randi([1, length(modFreqs)])) * 0.01;
    % Amplitude modulation with offset
    amp = (0.01 + sin(2 * pi * ampModFreq * t)) * (1/8/4);
    sig4 = sig4 + amp .* sin(2 * pi * freq * t);
end
soundsc(sig4, fs);

disp('All signals played.');