from myhdl import block, Signal, intbv, always_comb, always_seq, delay
import math

@block
def tone_generator(clk, reset, output):
    # Parameters
    NUM_OSC = 32
    SAMPLE_RATE = 48000
    TWO_PI = 2 * math.pi
    BIT_WIDTH = 16
    PHASE_WIDTH = 32
    AMPLITUDE = int(0.01 * (2**15))  # Scale for 16-bit fixed-point

    # Precomputed random frequencies (approximated as integers for hardware)
    osc_freq = [int(20 + i * 1980 / NUM_OSC) for i in range(NUM_OSC)]  # 20-2000 Hz
    lfo_freq = [int(0.01 + i * 0.09 / NUM_OSC * SAMPLE_RATE) for i in range(NUM_OSC)]  # 0.01-0.1 Hz

    # Signals
    phase = [Signal(intbv(0)[PHASE_WIDTH:]) for _ in range(NUM_OSC)]  # Oscillator phases
    lfo_phase = [Signal(intbv(0)[PHASE_WIDTH:]) for _ in range(NUM_OSC)]  # LFO phases
    output = Signal(intbv(0, min=-(2**15), max=2**15))  # 16-bit signed output

    # Sine LUT (simplified, 256 entries)
    SINE_LUT_SIZE = 256
    sine_lut = [int(math.sin(2 * math.pi * i / SINE_LUT_SIZE) * (2**15)) for i in range(SINE_LUT_SIZE)]

    @always_seq(clk.posedge, reset=reset)
    def process():
        sum_out = 0
        for i in range(NUM_OSC):
            # Update phases
            phase[i].next = (phase[i] + osc_freq[i]) % (2**PHASE_WIDTH)
            lfo_phase[i].next = (lfo_phase[i] + lfo_freq[i]) % (2**PHASE_WIDTH)

            # Compute sine using LUT
            phase_idx = (phase[i] >> (PHASE_WIDTH - 8))[:8]  # Map to LUT index
            lfo_idx = (lfo_phase[i] >> (PHASE_WIDTH - 8))[:8]
            osc_val = sine_lut[phase_idx]
            lfo_val = sine_lut[lfo_idx]
            sum_out += (osc_val * lfo_val * AMPLITUDE) >> 30  # Scale for fixed-point

        output.next = sum_out

    return process

# Simulation/testbench (optional)
@block
def testbench():
    clk = Signal(bool(0))
    reset = Signal(bool(0))
    output = Signal(intbv(0, min=-(2**15), max=2**15))

    inst = tone_generator(clk, reset, output)

    @always(delay(10))  # 48 kHz clock (approximate)
    def clkgen():
        clk.next = not clk

    return inst, clkgen

# Generate Verilog
if __name__ == "__main__":
    clk = Signal(bool(0))
    reset = Signal(bool(0))
    output = Signal(intbv(0, min=-(2**15), max=2**15))
    inst = tone_generator(clk, reset, output)
    inst.convert(hdl='Verilog', path='.')