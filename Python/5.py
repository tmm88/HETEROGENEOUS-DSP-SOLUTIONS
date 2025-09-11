from myhdl import Signal, ResetSignal, modbv, always_seq, block, instances

@block
def synth(clk, reset, left, right):
  NUM_VOICES = 5
  MAX_GRAINS = 32
  SR = 44100
  # Use modbv for fixed point (e.g., 32 bit, signed)
  freqs = [Signal(modbv(int(100 + i * 1000))[32:]) for i in range(NUM_VOICES)]
  modFreqs = [Signal(modbv(int(200 + i * 1000))[32:]) for i in range(NUM_VOICES)]
  density = Signal(modbv(int(100))[32:])
  scale = Signal(modbv(0)[32:])
  dust_counter = Signal(modbv(0)[32:])
  line_level = Signal(modbv(int(0.1 * 2**16))[32:])
  line_slope = Signal(modbv(int((20 - 0.1) * 2**16 / (5 * SR)))[32:])
  sin_phase = Signal(modbv(0)[32:])
  sin_inc = Signal(modbv(int(2 * 3.1416 * 20 / SR * 2**16))[32:])
  prev_trig = Signal(bool(0))
  lfsr = Signal(modbv(1)[32:])
  counter = [[Signal(modbv(0)[32:]) for _ in range(MAX_GRAINS)] for _ in range(NUM_VOICES)]
  car_phase = [[Signal(modbv(0)[32:]) for _ in range(MAX_GRAINS)] for _ in range(NUM_VOICES)]
  mod_phase = [[Signal(modbv(0)[32:]) for _ in range(MAX_GRAINS)] for _ in range(NUM_VOICES)]
  active = [[Signal(bool(0)) for _ in range(MAX_GRAINS)] for _ in range(NUM_VOICES)]
  @always_seq(clk.posedge, reset=reset)
  def logic():
    scale.next = density // SR // (2**16) # adjust for fixed
    lfsr.next = lfsr ^ (lfsr << 13) ^ (lfsr >> 17) ^ (lfsr << 5)
    trig = modbv(0)[32:]
    dust_counter.next = dust_counter - 1
    if dust_counter.signed() <= 0:
      dust_counter.next = lfsr // density # approximate
      trig = (lfsr - (1<<31)).signed() # approximate frand2
    line_level.next = line_level + line_slope
    if line_level > int(20 * 2**16):
      line_level.next = int(20 * 2**16)
    sin_phase.next = sin_phase + sin_inc
    if trig.signed() > 0 and not prev_trig:
      for v in range(NUM_VOICES):
        for g in range(MAX_GRAINS):
          if not active[v][g]:
            counter[v][g].next = int(0.02 * SR)
            car_phase[v][g].next = 0
            mod_phase[v][g].next = 0
            active[v][g].next = True
            break
    prev_trig.next = trig.signed() > 0
    mix = modbv(0)[32:]
    for v in range(NUM_VOICES):
      out = modbv(0)[32:]
      for g in range(MAX_GRAINS):
        if active[v][g]:
          mod = sin_lookup(mod_phase[v][g]) # implement lookup
          phase = car_phase[v][g] + (mod * line_level) // 2**16
          sig = sin_lookup(phase)
          fraction = (int(0.02 * SR) - counter[v][g]) // int(0.02 * SR)
          env = (1 << 15) + (1 << 15) - cos_lookup(fraction << 1) # approximate
          out = out + (sig * env) // 2**16
          mod_phase[v][g].next = mod_phase[v][g] + (2 * 3.1416 * modFreqs[v] // SR)
          car_phase[v][g].next = car_phase[v][g] + (2 * 3.1416 * freqs[v] // SR)
          counter[v][g].next = counter[v][g] - 1
          if counter[v][g] <= 0:
            active[v][g].next = False
      lo = -abs(sin_lookup(sin_phase))
      hi = abs(sin_lookup(sin_phase))
      folded = sc_fold_impl(out, lo, hi) # implement
      folded = (folded * int(0.1 * 2**16)) // 2**16
      mix = mix + folded
    left.next = mix
    right.next = mix
  return logic