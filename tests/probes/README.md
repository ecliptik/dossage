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

## pacesim -- standalone deadline-pacer + gettimeofday() paced-period capture

Answers: does dossage's game.cpp deadline pacer, run in total isolation
(no SDL, no rendering, no audio, no engine), reproduce the bimodal
fps_p50=16.67 (60ms) / fps_p95=9.09 (110ms) paced-period clusters that
every real-hardware run since patch `patches/passage/0035` has reported,
unmoved across 3 video chip vendors and 3 CPUs (2 Intel, 1 AMD, ~33%
clock-speed spread)? See `docs/PACER-TIMING-INVESTIGATION.md` for the
full question, evidence table, and why the other three probes here don't
already answer it.

**The mechanism this probe targets**, found by reading `game.cpp` itself
(not guessed): the DJGPP pacer and its RUNMANIFEST measurement use TWO
DIFFERENT CLOCKS. The absolute-deadline pacer (`game.cpp` ~1658-1847)
converges on `dosNextFrameDeadlineNS` using `SDL_GetTicksNS()` --
uclock()/PIT-timebase on this backend -- landing tightly on a true
~66.6667ms period. Patch 0035's RUNMANIFEST capture (`game.cpp`
~1857-1888) measures the delta between consecutive frame timestamps using
`Time::getCurrentTime()` -- gettimeofday(), ms-truncated -- a completely
separate clock read from the one the pacer just converged against. The
hypothesis: if gettimeofday() free-runs at some granularity coarser than
the pacer's convergence precision (a BIOS/PIT tick, ~54.9255ms, is the
working hypothesis in the investigation brief -- 2x that is 109.85ms, a
close match for the observed ~110ms cluster), a smooth, tightly-converged
~66.67ms period would be measured by gettimeofday() as one of a small
fixed set of values, determined by clock-boundary PHASE rather than
anything about the real hardware -- explaining both why there are exactly
two clusters and why they don't move with CPU speed/vendor/video card.

This is deliberately NOT the same question `clkdrift.c` already answered
(whether gettimeofday() and uclock() agree on AVERAGE RATE -- they do).
It is whether gettimeofday()'s SAMPLE-TO-SAMPLE behavior across a
tightly-converged ~66.67ms interval, produced by the SAME deadline-pacer
structure and constants `game.cpp` actually uses, shows discrete
clustering. Rate agreement on average is entirely compatible with a
coarse per-sample quantization.

**Fidelity notes** (see the file header for the full list): the deadline-
pacer structure and constants (`dosFrameTickNS`, `dosFrameMaxDebtNS`,
the 3ms spin-handoff, the 0.2ms yield-floor) are reproduced verbatim from
`game.cpp`. `SDL_GetTicksNS()` is approximated as uclock() rescaled via
`UCLOCKS_PER_SEC` -- valid for dossage specifically, since it never runs
the SDL/0122 GUS/AdLib pump timebase that would otherwise change this.
The pacer's final-approach loop (`SDL_Delay(0)`, DOS_Yield()-only) has no
standalone equivalent outside SDL/DPMI cooperative scheduling and is
substituted with a tight uclock() spin-poll -- a fidelity gap for
absolute interval-length claims, not for the phase-vs-gettimeofday
question this probe asks. No SB16 DMA audio IRQs, no rendering, no event
polling (same class of caveat as `dlygran.c`/`clkdrift.c`). Per-frame
"work" is an optional argv-configurable busy-wait (default 0) so a
work-free run can be extended with representative work without a
rebuild.

- Source: `pacesim.c`
- Binary: `build/probes/PACESIM.EXE` (via `make probe-pacesim`)
- Log: `PACESIM.LOG` by default, or `<logtag>.LOG` if a third argument is
  given, written to the working directory the probe runs in
- Launcher: `pacesim.bat` (copy both files, plus CWSDPMI.EXE, to the
  target's working dir)
- Args: `PACESIM.EXE [niter] [work_ms] [logtag]` -- niter default 3000
  (range 100-50000, ~200s of simulated real time at steady state),
  work_ms default 0 (range 0-200), logtag optional (alnum/underscore,
  truncated to 8 chars -- writes `<LOGTAG>.LOG` instead of `PACESIM.LOG`)
- **Truncate-mode gotcha, hit for real on the first real-hardware round**:
  like `dlygran.c`/`clkdrift.c`/`clkscale.c`, the log is opened in `"w"`
  (truncate), not `"a"` -- deliberately kept consistent with those three
  rather than switched to append, since an unmarked append would silently
  concatenate unrelated runs into one file with no boundary marker. A
  WORKMS=0 pass followed by a WORKMS=45 pass in the same directory with
  no logtag on either invocation silently destroyed the first pass's
  data before it could be retrieved. Fix: pass a distinct `logtag` per
  pass (e.g. `PACESIM.EXE 3000 0 W0` then `PACESIM.EXE 3000 45 W45` ->
  `W0.LOG` and `W45.LOG`, both retrievable afterward), or copy
  `PACESIM.LOG` out between passes. The probe also checks for a
  same-named file before truncating and prints a warning to stderr if
  one already exists, so a same-name clobber is loud rather than silent
  when a distinct logtag isn't used -- though see the DOSBox-X note
  below for why that warning couldn't be captured in the emulator smoke.
- Output includes every raw paced-period sample, min/median/mean/p95/max,
  a 10ms-bucket histogram (bimodal shape visible without post-processing),
  pacer branch counts, reject counts (same filter as
  `DOS_PORT_FRAME_TIME_MIN_PLAUSIBLE_S`), and fps_p50/fps_p95 computed
  EXACTLY as `game.cpp`'s RUNMANIFEST emission does -- directly comparable
  to the real RUNMANIFEST lines in `docs/benchmarks/`.

**DOSBox-X note**: DOSBox-X smoke (150 iterations, `--fast`) confirms the
probe builds, runs, writes a parseable log with internally-consistent
arithmetic (reject counts, branch counts, and sample_count all reconcile
against niter), and does not crash. It also, structurally, reproduces
clusters at the same 50/60/110ms values the real-hardware anomaly
reports -- but this was NOT reported as a finding at the time: DOSBox-X's
own PIT/gettimeofday() emulation is documented elsewhere in this file as
coarse and non-representative, and this run's own numbers confirmed that
directly -- a 46% reject rate and 69/150 "resync ahead by more than a
frame" events, against real hardware's <0.3% reject rate on the
equivalent RUNMANIFEST capture. Per this repo's standing rule, DOSBox-X
is a correctness/mechanism gate only, never a performance proxy.

Separately: verifying the pre-truncate stderr warning (above) under
DOSBox-X headless capture ran into an unrelated, pre-existing limitation
-- real DOS `COMMAND.COM` does not understand `N>` (numbered-handle)
redirection syntax at all, only plain `>`/`>>`/`<`, so a batch line like
`PACESIM.EXE ... 2> ERR.TXT` does not do what it looks like it does: `2`
becomes a literal token DJGPP's own arg/redirection parser doesn't
recognize either, and the LAST bare `>` on the line wins for stdout,
leaving an earlier `>` target created but empty. This is the same class
of issue as this repo's `dosbox-run.sh --merge-stderr` flag (see
`docs/` / that script's own header comment claiming DJGPP handles
`2>&1` -- observed behavior here says otherwise; worth a note to
whoever maintains that shared script, not something to fix from this
port-local README). Not a pacesim bug -- the plain-`>`-only invocations
used for the actual argv[3]/logtag verification above worked correctly.

**Real-hardware result (2026-09-03, Am5x86-133 + Mach64, via vcctrl-c3)**:
both requested passes ran. Pass 2 (WORKMS=45, complete: n=2992,
reject_rate=0.2667% -- close to real game runs' 0.27-0.29%) reproduced
fps_p50=16.67/fps_p95=9.09 exactly, with a histogram showing two islands
(a ~40-70ms cluster and a ~90-120ms cluster, near-total gap 70-90ms).
Checking the actual raw per-sample data (not just the aggregated
histogram) against a falsifiable exact-10ms-multiple prediction found
zero exceptions across all 256 printed samples -- every value is exactly
one of {50, 60, 100, 110, 330}ms; the apparent island "width" is a
histogram-bucket-boundary artifact (bucket edges sit exactly on the true
values, so ordinary float/truncation noise scatters samples into the
adjacent decade bucket), not real spread. Pass 1 (WORKMS=0, partial --
lost to the truncate-mode gotcha above before the logtag fix landed)
matched on every figure that could be compared: same fps_p50/fps_p95,
similar band fractions. See `docs/PACER-TIMING-INVESTIGATION.md` for the
full writeup, root-cause verdict, and the exact mechanism (traced to
DOS's INT 21h AH=2Ch hundredths-of-a-second clock).

**A real bug this same raw-data check surfaced, since fixed**: pass 2's
reported mean (68.2152ms) was measurably inflated by this probe's OWN
per-line `fsync()` during its first 200 iterations (full resolution)
versus every-50th afterward -- each `fsync()` call happens inside the
timed measurement loop, so 200 densely-packed fsync calls measurably
perturbed exactly the iterations that paid for them (52% landed in the
~110ms/2-tick band during i<200 vs. 23% after, and both of the run's
~330ms stall outliers landed at i=80/i=196, both inside that window).
Fixed: fsync is now throttled to a uniform per-call cadence
(`FSYNC_EVERY_N_CALLS` in `pacesim.c`) instead of tracking the raw-sample
print density, with an explicit final sync on every exit path so
durability at the end of a run is unaffected. Not yet re-verified on real
hardware post-fix (the analytical/data-driven case for the diagnosis is
strong enough that this wasn't treated as blocking -- see
`docs/PACER-TIMING-INVESTIGATION.md`'s RESOLVED section for the full
reasoning); a future real-hardware pass would be expected to show a mean
closer to the analytically-predicted ~66.68ms.
