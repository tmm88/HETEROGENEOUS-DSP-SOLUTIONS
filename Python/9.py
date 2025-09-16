from myhdl import block, Signal, intbv, always_seq, always_comb, instance, delay
import numpy as np
from random import choice

# Parameters
SAMPLE_RATE = 44100  # Hz
NUM_OSCILLATORS = 8
FREQ_CHOICES = [3.14 * x for x in [0.5, 1, 2]]  # LFO frequencies: [1.57, 3.14, 6.28] Hz
BASE_FREQ = 47.0 + np.random.uniform(0, 1)  # Random between 47-48 Hz
MOD_DEPTH = 1.0 / (8 * 4)  # 0.03125
BIT_WIDTH = 16  # Fixed-point precision for signals
MAX_VALUE = 2**(BIT_WIDTH-1) - 1  # Max for signed intbv
CLK_FREQ = 100e6  # 100 MHz FPGA clock for example
SAMPLES_PER_CYCLE = int(CLK_FREQ / SAMPLE_RATE)  # Clock cycles per audio sample

# Sine table for lookup (precomputed for hardware efficiency)
SINE_TABLE_SIZE = 1024
SINE_TABLE = [int(np.sin(2 * np.pi * i / SINE_TABLE_SIZE) * MAX_VALUE) for i in range(SINE_TABLE_SIZE)]

@block
def fm_synth(clk, reset, output_left, output_right):
    # Signals for phase accumulators and outputs
    phase_acc = [Signal(intbv(0)[32:]) for _ in range(NUM_OSCILLATORS)]  # Phase accumulators
    lfo_phase = [Signal(intbv(0)[32:]) for _ in range(NUM_OSCILLATORS)]  # LFO phase accumulators
    mixed_signal = Signal(intbv(0, min=-MAX_VALUE*NUM_OSCILLATORS, max=MAX_VALUE*NUM_OSCILLATORS+1))
    sample_counter = Signal(intbv(0)[16:])  # Counter for sample rate

    # Precompute LFO frequencies (fixed at synthesis time)
    lfo_freqs = [choice(FREQ_CHOICES) for _ in range(NUM_OSCILLATORS)]
    phase_inc = [(int((freq / SAMPLE_RATE) * SINE_TABLE_SIZE * 2**32) & 0xFFFFFFFF)
                 for freq in [BASE_FREQ + MOD_DEPTH * np.sin(2 * np.pi * f) for f in lfo_freqs]]

    @always_seq(clk.posedge, reset=reset)
    def synth_logic():
        # Update sample counter
        sample_counter.next = sample_counter + 1
        if sample_counter >= SAMPLES_PER_CYCLE - 1:
            sample_counter.next = 0
            
            # Update phase accumulators
            for i in range(NUM_OSCILLATORS):
                lfo_phase[i].next = (lfo_phase[i] + int((lfo_freqs[i] / SAMPLE_RATE) * SINE_TABLE_SIZE * 2**32)) & 0xFFFFFFFF
                lfo_value = SINE_TABLE[lfo_phase[i][31:32-BIT_WIDTH]]  # Lookup LFO value
                mod_freq = int(BASE_FREQ + MOD_DEPTH * lfo_value / MAX_VALUE)
                phase_inc[i] = int((mod_freq / SAMPLE_RATE) * SINE_TABLE_SIZE * 2**32) & 0xFFFFFFFF
                phase_acc[i].next = (phase_acc[i] + phase_inc[i]) & 0xFFFFFFFF
            
            # Mix oscillators
            mixed = 0
            for i in range(NUM_OSCILLATORS):
                table_idx = phase_acc[i][31:32-BIT_WIDTH]  # Map phase to sine table index
                mixed = mixed + SINE_TABLE[table_idx]
            
            mixed_signal.next = mixed // NUM_OSCILLATORS  # Normalize

    @always_comb
    def output_logic():
        output_left.next = mixed_signal  # Stereo output
        output_right.next = mixed_signal

    return synth_logic, output_logic

# Simulation and Verilog generation
@block
def tb_fm_synth():
    clk = Signal(bool(0))
    reset = Signal(bool(1))
    output_left = Signal(intbv(0, min=-MAX_VALUE, max=MAX_VALUE+1))
    output_right = Signal(intbv(0, min=-MAX_VALUE, max=MAX_VALUE+1))

    # Instantiate the synthesizer
    dut = fm_synth(clk, reset, output_left, output_right)

    @instance
    def stimulus():
        # Reset sequence
        reset.next = 1
        yield delay(20)
        reset.next = 0
        yield delay(20)

        # Simulate for a few samples
        for _ in range(1000):
            clk.next = 1
            yield delay(5)  # Half clock cycle (100 MHz -> 10 ns period)
            clk.next = 0
            yield delay(5)
            print(f"Left: {output_left}, Right: {output_right}")

    return dut, stimulus

# Run simulation and generate Verilog
def main():
    tb = tb_fm_synth()
    tb.convert(hdl='Verilog', path='output')  # Generate Verilog in 'output' directory
    print("Verilog code generated in 'output/fm_synth.v'")

if __name__ == '__main__':
    main()
