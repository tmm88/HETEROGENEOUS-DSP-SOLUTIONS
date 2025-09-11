from myhdl import block, always_seq, Signal, intbv, delay, always_comb
import random  # For simulation

@block
def LFNoise1(clk, reset, out, rate):
    curr = Signal(intbv(0, min=-1000, max=1000))  # Scaled fixed point
    target = Signal(intbv(0, min=-1000, max=1000))
    phase = Signal(intbv(0, min=0, max=44100))  # Assume SR=44100
    inc = Signal(intbv(0, min=-2000, max=2000))

    @always_seq(clk.posedge, reset=reset)
    def logic():
        phase.next = phase + rate  # Rate scaled
        if phase >= 44100:
            phase.next = 0
            curr.next = target
            target.next = random.randint(-1000, 1000)  # Random for sim
            inc.next = target - curr
        out.next = curr + inc * (phase // (44100 // 100))  # Approximate

    return logic

@block
def Saw(clk, reset, out, freq):
    phase = Signal(intbv(0, min=0, max=1000))  # 0-1 scaled *1000

    @always_seq(clk.posedge, reset=reset)
    def logic():
        phase.next = (phase + freq) % 1000  # Freq scaled
        out.next = (phase * 2 // 1000) - 1  # Saw

    return logic

# Simple reverb, delay line
@block
def SimpleReverb(clk, reset, in_sig, out, roomsize):
    buffer = [Signal(intbv(0)[16:]) for _ in range(10000)]
    idx = Signal(intbv(0, min=0, max=10000))

    @always_seq(clk.posedge, reset=reset)
    def logic():
        out.next = buffer[idx] + in_sig // 2  # Simple
        buffer[idx].next = in_sig + out // 2
        idx.next = (idx + 1) % roomsize

    return logic

# Top module
@block
def AmbientDrone(clk, reset, outL, outR):
    freq_out = Signal(intbv(0)[16:])
    detune = [Signal(intbv(0)[16:]) for _ in range(6)]
    saw_out = [Signal(intbv(0)[16:]) for _ in range(6)]
    rev_out = [Signal(intbv(0)[16:]) for _ in range(6)]
    sound = Signal(intbv(0)[16:])

    freq_noise = LFNoise1(clk, reset, freq_out, Signal(intbv(int(3.14*2*100))))  # Scaled

    detune_noises = [LFNoise1(clk, reset, detune[i], Signal(intbv(10))) for i in range(6)]  # 0.1 *100

    saws = [Saw(clk, reset, saw_out[i], (freq_out + detune[i])) for i in range(6)]

    reverbs = [SimpleReverb(clk, reset, saw_out[i], rev_out[i], Signal(intbv(5000))) for i in range(6)]

    @always_comb
    def mix():
        temp = 0
        for i in range(6):
            temp += rev_out[i] // 10
        sound.next = temp // 2  # *0.6 approx
        outL.next = sound
        outR.next = sound

    return freq_noise, detune_noises, saws, reverbs, mix

# For simulation
# Add testbench code