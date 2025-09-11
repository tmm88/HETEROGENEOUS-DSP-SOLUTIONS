from myhdl import *

NUM_INST = 16
SR = 44100
REVERB_SIZE = 22050

@block
def synth(clk, reset, output):

    phase = [Signal(intbv(0)[32:]) for _ in range(NUM_INST)]
    env = [Signal(fixbv(0.0, min=0, max=1, res=1.0/2**16)) for _ in range(NUM_INST)]
    reverb_buffer = [Signal(fixbv(0.0, min=-1, max=1, res=1.0/2**16)) for _ in range(REVERB_SIZE)]
    reverb_idx = Signal(intbv(0)[15:])

    trigger_counter = Signal(intbv(0)[16:])
    trigger_rate = intbv(SR // 10)[16:]

    # Sin lookup table (simple 256 entry)
    sin_lut = [Signal(fixbv(math.sin(2*math.pi*i/256), min=-1, max=1, res=1.0/2**16)) for i in range(256)]

    @always_seq(clk.posedge, reset=reset)
    def logic():
        sum_val = fixbv(0.0, min=-NUM_INST, max=NUM_INST, res=1.0/2**16)

        for i in range(NUM_INST):
            freq = intbv(32 + i)[16:]
            phase_inc = intbv((freq << 16) // SR)[32:]

            if trigger_counter == 0:
                env[i].next = 0

            phase[i].next = phase[i] + phase_inc

            lut_idx = phase[i][31:24]  # Top 8 bits for LUT
            osc = sin_lut[lut_idx]

            if env[i] < 1.0:
                env[i].next = env[i] + 100.0 / SR  # Attack
            else:
                env[i].next = env[i] - 1.0 / SR  # Release
                if env[i] < 0:
                    env[i].next = 0

            signal = osc * env[i]

            rev = reverb_buffer[reverb_idx]
            reverb_buffer[reverb_idx].next = signal + rev * 0.7
            sum_val = sum_val + rev / NUM_INST

        reverb_idx.next = (reverb_idx + 1) % REVERB_SIZE
        trigger_counter.next = (trigger_counter + 1) % trigger_rate.val

        output.next = sum_val

    return logic

# To convert to Verilog
clk = Signal(bool(0))
reset = ResetSignal(0, active=1, isasync=True)
output = Signal(fixbv(0.0, min=-1, max=1, res=1.0/2**16))

inst = synth(clk, reset, output)
inst.convert(hdl='Verilog')