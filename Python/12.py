import cupy as cp
import cusignal
import numpy as np
import sounddevice as sd

# Parameters
sr = 48000       # sample rate
dur = 10         # seconds
t = cp.linspace(0, dur, int(sr * dur), endpoint=False)

# ---------- FM DRONE ----------
# Modulator frequency with wandering LFNoise-like behavior
mod_freq = 50 * (1 + 0.5 * cp.sin(2 * cp.pi * 0.2 * t))  # like LFNoise2.kr
mod_index = 40 + 20 * cp.sin(2 * cp.pi * 0.1 * t)        # like LFNoise1.kr

# Carrier frequencies
carriers = [60, 62, 90]

# FM synthesis per carrier
fm_signals = []
for cfreq in carriers:
    mod = cp.sin(2 * cp.pi * mod_freq * t) * mod_index
    fm = cp.sin(2 * cp.pi * (cfreq * t) + mod)
    fm_signals.append(fm)

# Mix and add sub oscillator
drone = cp.sum(cp.stack(fm_signals), axis=0)
sub = cp.sin(2 * cp.pi * 30 * t)
sig = drone * 0.1 + sub * 0.1

# ---------- Filtering ----------
# Emulate LFNoise filter sweep
cutoff = 1000 + 500 * cp.sin(2 * cp.pi * 0.05 * t)  # dynamic cutoff
b, a = cusignal.butter(4, 1500 / (sr / 2), btype='low')
sig = cusignal.lfilter(b, a, sig)

# ---------- Nonlinear distortion ----------
sig = cp.tanh(sig * 5) * 0.3

# ---------- Reverb (convolution with small IR) ----------
ir = cp.exp(-cp.linspace(0, 1, 4000))  # exponential decay IR
sig = cusignal.fftconvolve(sig, ir, mode='same')

# ---------- Stereo spread ----------
stereo = cp.stack([sig * 0.8, sig * 1.0], axis=1)

# Back to CPU for playback
stereo_cpu = cp.asnumpy(stereo)

# Play
sd.play(stereo_cpu, sr)
sd.wait()