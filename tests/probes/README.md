# DOSSAGE-local diagnostic probes

Standalone DJGPP probes genuinely specific enough to this port's own
investigation to live here rather than in the hub's shared library
(`.sdl-dos-ports/shared/tests/probes/`) -- see that directory's own
README first; a generic hardware/platform question likely already has a
probe there. Check there before adding one here.

Build via `make probes` (or `make probe-<name>` for a single one) from
the repo root -- see `probes.mk`. This is a dedicated target, separate
from the game build (`make game`): pure DJGPP libc + DPMI, no SDL, no
engine, no C++. Binaries land in `build/probes/<NAME>.EXE`.

## dlygran -- delay(1) granularity vs. tight uclock() poll

Answers: is DJGPP libc's `delay(1)`, as called from the shared SDL3-DOS
backend's `SDL_SYS_DelayNS()` frame-limiter wait loop
(`src/timer/dos/SDL_systimer.c`, confirmed live in
`.sdl-dos-ports/shared/patches/sdl3-dos/0122-*.patch`), millisecond-fine
or coarse/BIOS-tick-quantized (~54.9ms/18.2Hz)? This is the leading
hypothesis for DOSSAGE's ~1fps real-hardware KPI gap (see PLAN.md) --
prior bisection proved real per-frame work has ~20ms of headroom against
the 15fps budget, and the entire remaining gap is believed to be
`SDL_Delay()` overshoot inside that wait loop's `delay(1)` calls.

Not a generic hardware question in the usual sense (it's not "what does
this video/sound card do"), but it IS entirely engine/game-agnostic --
any port using the shared SDL3-DOS backend hits the same call site. It
lives here for now (this port's own `tests/probes/`) rather than in the
hub's `shared/tests/probes/` per this repo's own CLAUDE.md (`.sdl-dos-
ports/` is read-only from here); once real-hardware results land,
whoever owns the hub repo can decide whether a cleaned-up version belongs
in the shared library.

- Source: `dlygran.c`
- Binary: `build/probes/DLYGRAN.EXE` (via `make probe-dlygran`)
- Log: `DLYGRAN.LOG`, written to the working directory the probe runs in
- Launcher: `dlygran.bat` (copy both files to the target's working dir)

**CAVEAT**: this probe measures `delay(1)` in isolation, with no SB16-
compatible DMA audio IRQs running. The real game has those firing
continuously during every frame. If `delay(1)` is HLT-based and wakes on
ANY interrupt (not just PIT/IRQ0), the real in-game number could be
*shorter* than what this probe measures. Treat DLYGRAN's numbers as an
upper-bound / first-pass answer to "is this mechanism coarse at all," not
necessarily the exact in-game distribution.

**DOSBox-X note**: DOSBox-X smoke-testing this probe confirms it builds,
runs, and produces a sane log -- correctness only. DOSBox-X's own
delay()/PIT emulation is not representative of real-hardware BIOS-tick
behavior; per this repo's rule, DOSBox-X is never a performance proxy.
The real answer comes only from a real-hardware run (486DX2-66 rig, via
vcctrl).

## clkdrift -- gettimeofday() vs uclock() rate agreement

Answers: does minorGems' `Time::getCurrentTime()` (used by game.cpp's
frame limiter for `frameTime`, backed by DJGPP's `gettimeofday()` with
`tv_usec` truncated to whole ms -- see `vendor/minorgems/system/dos/
TimeDOS.cpp`) tick at the SAME real rate as `uclock()` (DJGPP's ~1.19MHz
PIT-based clock, what `SDL_Delay()`'s own wait loop measures against)?

The DOS_Yield()-instrumentation round that eliminated hypothesis (2) in
dlygran.c's history (see PLAN.md) also surfaced an unreconciled ~4.4ms/
frame gap between the algebraically-predicted steady-state fps (from
game.cpp's `extraTime = 1.0/lockedFrameRate - frameTime` identity, where
`frameTime` itself cancels out) and the actual measured real-hardware
fps. Because `frameTime` is measured via `gettimeofday()` while
`SDL_Delay()` measures its own sleep via `uclock()`, a genuine RATE
mismatch between those two clocks -- not just noise -- would corrupt
every `target` the game computes, invisible to any measurement confined
to the `SDL_Delay()` call alone. This probe distinguishes that from plain
+/-1ms floor-truncation noise by checking whether the gettimeofday()-vs-
uclock() discrepancy grows proportionally with interval length (rate
mismatch) or stays flat (quantization noise only, dead lead).

- Source: `clkdrift.c`
- Binary: `build/probes/CLKDRIFT.EXE` (via `make probe-clkdrift`)
- Log: `CLKDRIFT.LOG`, written to the working directory the probe runs in
- Launcher: `clkdrift.bat` (copy both files to the target's working dir)

**DOSBox-X note**: DOSBox-X smoke-testing confirms the probe builds,
runs, and produces a sane log -- correctness only. DOSBox-X's own
`gettimeofday()`/PIT emulation is visibly coarse (tens-of-ms jumps
observed in the DOSBox-X smoke run) and is NOT representative of real
hardware; per this repo's rule, DOSBox-X is never a performance proxy.
The real answer comes only from a real-hardware run (486-class rig, via
vcctrl).

## clkscale -- uclock() vs time(NULL) rate scale factor

Answers: is the frame pacer's ruler wrong, or is the fps report's ruler
wrong? The absolute-deadline pacer targets 66.6667ms/frame measured on
`uclock()` (via SDL_GetPerformanceCounter, with
SDL_GetPerformanceFrequency reporting `UCLOCKS_PER_SEC` = 1193180), while
the game's own exit fps report measures with a DIFFERENT clock entirely
-- `frameCount / (time(NULL) - startTime)`, the BIOS/DOS time-of-day.
Real hardware reports 14.746032 fps; 15.0 / 14.746032 = 1.01722 exactly,
a delivered period of 67.815ms against an intended 66.6667ms, a CONSTANT
1.72%. Removing 1.23ms/frame of genuine per-frame work moved the measured
period by only 0.086ms, so the pacer is demonstrably clamping the period
-- its ruler is just calibrated wrong. So either (a) `uclock()` runs slow
relative to real time, or (b) `time(NULL)` runs fast; every prior
measurement is structurally blind to both, because they all used the same
clock family.

This repo's `clkdrift.c` compared `gettimeofday()` against `uclock()` and
found them consistent -- but both are PIT-derived, so it could not have
caught this. `clkscale` deliberately brings in `time(NULL)` and the CMOS
RTC, which are not on that leg.

**Method**: `time(NULL)` has 1-second granularity and the effect is only
1.72%, so naive endpoint sampling would carry +/-1.7% error and prove
nothing. Instead both endpoints are pinned to true second BOUNDARIES by
busy-polling for the tick-over, reducing boundary error to the poll period
(microseconds). The window defaults to 120s (argv override, 30..600).
Per-second uclock tick counts are also reported as a distribution, which
separates a steady scale factor from jitter or periodic correction.

**The CMOS RTC leg (beyond the original brief)**: `uclock()`,
`gettimeofday()` and the BIOS tick are all downstream of the same
14.31818MHz oscillator via PIT channel 0, so a probe limited to those
three can only show that they DISAGREE, never which one is lying. The
CMOS RTC runs off a separate 32.768kHz crystal that the PIT cannot
influence, so its seconds register is an independent real-time reference
already present on the machine. If it reads cleanly it ATTRIBUTES the
error (uclock matches RTC -> hypothesis (b); RTC matches time(NULL) ->
hypothesis (a)). Verified in the self-test below: hypotheses (a) and (b)
produce an IDENTICAL uclock-vs-time(NULL) ratio and are separable ONLY by
the RTC leg.

**Why the RTC is the arbiter, and host timestamps are not**: `uclock()`
and the BIOS tick share the PIT crystal (BIOS tick = crystal / 65536 =
18.2065 Hz), so a 1.72% divergence between them cannot be a crystal
difference -- it has to be a counting or arithmetic error in one of them,
and only a clock on a *different* crystal can say which. Host timestamps
were considered and rejected: the rig operator's bracket carries 1-4s of
slop (SSH round-trip plus uncertainty about when DOS-side init ends),
while the two hypotheses separate the elapsed figures by only ~2.1s over a
120s window. `CLKSCALE-BEGIN` / `CLKSCALE-END` are still printed as a
sanity check on gross error, but they are not the arbiter.

If the RTC leg fails, the probe reports the disagreement, says plainly
that it could not attribute it, and computes the window length at which
the host bracket *would* become usable (~709s at the observed effect
size -- which is why `WINDOW_MAX` is 900s rather than 600s). It does not
guess.

**BIOS-tick conflation check**: the observed ratio 1.01722 is within
0.01% of 54.9255/54 (the true BIOS tick period vs a naive "54ms"
assumption), so the probe reports that pattern match explicitly whenever
a disagreement is found. It is flagged in the log as a numeric
coincidence of the right size, NOT evidence of mechanism, and it takes no
part in the attribution logic.

**Resolution gate**: the verdict is gated on the run's own worst-case
boundary uncertainty. If that is not small against the effect, the probe
reports INCONCLUSIVE (resolution-limited) and recommends a longer window
rather than emitting a confident reading -- because insufficient
resolution counterfeits precisely a "the clocks agree" result.

- Source: `clkscale.c`
- Binary: `build/probes/CLKSCALE.EXE` (via `make probe-clkscale`)
- Log: `CLKSCALE.LOG`, written to the working directory the probe runs in
- Launcher: `clkscale.bat` (copy both files to the target's working dir)
- Runtime: ~2 minutes at the default window; silent while measuring (the
  probe performs no disk I/O inside the window, since a blocking write
  could stretch a poll past a second boundary)

**Self-test**: because a "it ran and wrote a log" smoke cannot tell a
correct scale factor from a plausible-looking wrong one, the file carries
a guarded host-only harness that replaces the four clocks with a simulated
timebase of known scale:

```
gcc -O2 -Wall -Wextra -DCLKSCALE_SELFTEST -o /tmp/clkscale_test clkscale.c -lm
CLKSCALE_SIM_U=0.9830688 CLKSCALE_SIM_T=1.0 CLKSCALE_SIM_R=1.0 /tmp/clkscale_test 30  # -> (a)
CLKSCALE_SIM_U=1.0 CLKSCALE_SIM_T=1.01722 CLKSCALE_SIM_R=1.0 /tmp/clkscale_test 30    # -> (b)
CLKSCALE_SIM_U=1.0 CLKSCALE_SIM_T=1.0 CLKSCALE_SIM_R=1.0 /tmp/clkscale_test 30        # -> falsified
CLKSCALE_SIM_STEP=0.02 /tmp/clkscale_test 30                                          # -> resolution gate fires
CLKSCALE_SIM_NORTC=1 CLKSCALE_SIM_U=0.9830688 /tmp/clkscale_test 120                  # -> dead-RTC fallback
```

Scenario (a) yields `RTC vs time(NULL) = 1.000000` (RTC and BIOS tick
agree, uclock is the outlier); scenario (b) yields `0.983071` (uclock
matches the RTC, the BIOS tick is the outlier). That ratio is stated
directly in the log because it is independent of `uclock()` and of
`UCLOCKS_PER_SEC` entirely.

Scenario (a) recovers the injected factor to 2e-6 and derives a 67.8149ms
period against the 67.815ms observed on real hardware. This exercises the
shipped analysis/verdict code, not a replica.

**DOSBox-X note**: DOSBox-X smoke confirms the probe builds, runs, writes
a parseable log, emits both markers and produces a full analysis --
correctness only. DOSBox-X slaves its PIT, BIOS tick AND RTC all to host
time, so it reports near-perfect agreement no matter what real hardware
does; its per-DOS-call cost is also ~100ms of emulated time, which trips
the resolution gate. Neither is representative. Per this repo's rule,
DOSBox-X is never a performance proxy -- the real answer comes only from
a real-hardware run (486DX2-66 rig, via vcctrl).
