# Timing

Never assume display FPS and simulation/game timing must be identical.
doskutsu demonstrated why this matters: on slow real hardware, render rate
varies a lot (see `HARDWARE.md`'s baseline table), but game logic must
continue at its intended rate regardless.

## Determine, per port

```
simulation tick
animation tick
audio tick
render tick
input polling rate
```

Where possible:

```
simulation = fixed timestep
rendering  = as fast as practical
```

This prevents slow rendering from slowing gameplay — a candidate does not
need to hit 50/60 FPS to be considered playable if its simulation timing is
cleanly separated from its render rate.

## Your clock can lie

DJGPP's `uclock()` — the only timebase behind `SDL_GetTicks`/`SDL_Delay`/
`SDL_GetPerformanceCounter` on this backend — reads PIT channel 0. Any
code that reprograms PIT ch0 for its own purposes (a PIT-driven audio pump
is the confirmed real case — see the "PIT-driven audio pump" hazard in
`docs/audio.md`'s architecture-wide section) can freeze or corrupt this
timebase while active, silently
invalidating any performance measurement taken during that window — this
produced a false ~42% fps-cost finding for a feature that, measured
correctly via flip-counts against wall-clock `time()` instead, cost
nothing. Before trusting a timing measurement that looks surprising,
check whether anything active during that window reprograms a PIT channel
DJGPP's clock depends on.

A related but distinct `uclock()` hazard: it is **not monotonic** on this
platform — real-hardware logs have shown the unsigned-subtraction wraparound
that produces when two `uclock()` reads are subtracted in the wrong order
(a duration coming out near `2^32`, i.e. ~1.8×10^19 in a wider integer
type, instead of the small positive number expected). Clamp any
`uclock()`-derived duration to a sane range before trusting it, and treat
a single wildly-anomalous timing sample as a suspect measurement to
discard, not as a finding to explain.

**A busy-wait fix can starve the BIOS timer interrupt, and the fps report
will lie about it in the flattering direction.** dossage/Passage's
frame-limiter investigation (see `docs/optimization.md`) tried replacing a
`delay(1)` call with a tight busy-wait poll. On real hardware this
starved the BIOS timer interrupt chain -- `time(NULL)` lost 54% of its
ticks during the run. Because Passage's own fps report divides frame
count by `time(NULL)`, **the reported number went UP (14.3) as real
performance collapsed (~6.5 actual, confirmed via vcctrl's independent
external wall-clock bracket)**. The general rule this is a concrete case
study for: **a metric computed from a clock the change under test can
perturb will tend to fail in the flattering direction, not an obviously-
wrong one** -- the busy-wait didn't just make the number noisy, it made a
badly broken run *look healthy*. Only an external witness (a measurement
that doesn't share the compromised clock) can catch this; nothing
DOS-side could have, since every DOS-side clock was corrupted by the same
busy-wait. This is precisely why this hub mandates independent/
real-hardware verification over trusting an engine's own self-report --
see `docs/video.md`'s triage-order note for the same discipline applied
to a different failure class.

**A corollary that cost real rig time before it was caught: an
external-witness check has to bracket the same interval the engine is
actually measuring, or it manufactures false failures.** The validity
check built for the above ("wall-clock and the engine's own game-time
should agree within a few seconds") was itself mis-specified -- it
silently assumed zero startup time, but a 486 spends ~14-16s on DOS init,
CWSDPMI, PicoGUS, VESA mode-set, and asset loading before the engine's
own timer even starts. That gap made the check fire on a genuinely
healthy run. The harness applied the check correctly; the check's own
interval definition was wrong. Before trusting an external-witness check
as evidence of a regression, confirm its two endpoints bracket the exact
same span the in-engine measurement covers -- an unaccounted-for
startup/teardown window on one side of the comparison but not the other
produces a confident, wrong verdict in either direction.

## SDL_Delay() overshoot on 486-class CPUs (resolved: delay(1) granularity, not clock rate)

dossage/Passage bisected a residual fps gap on 486DX2-66 + S3 ViRGE (see
`docs/optimization.md`) to `SDL_Delay()` timer-precision overshoot, with
real per-frame render work well under budget (~45.6ms against a 66.7ms
frame budget). The original hypothesis here -- a PIT-channel-0 clock-rate
divergence from nominal, possibly tied to no-`RDTSC` 486-class hardware --
was tested directly and **falsified**: `uclock()`, `time(NULL)`, and the
CMOS RTC (read directly via ports 0x70/0x71, independent of any BIOS/PIT
chain) all agreed to within 0.03%, nowhere near the ~1.7% a clock-rate
mismatch would require.

**Actual root cause**, found only once the frame-limiter's own decisions
were instrumented directly: `SDL_SYS_DelayNS`'s wait loop calls DJGPP's
`delay(1)`, which genuinely takes ~1.236ms per call (a fine-grained,
non-BIOS-tick-quantized measurement -- the *earlier* hypothesis that
`delay(1)` itself was coarsely quantized was correctly eliminated), but
the loop can only re-check its exit condition *between* `delay(1)` calls
-- so a sleep can land up to a full ~1.236ms quantum later than its
deadline. **"Resolved" here means the overshoot *mechanism* is settled, not that
any specific fps KPI has been hit** -- that's a separate, still-open
question tracked in `docs/optimization.md`, since the residual gap
between steady-state and a reported number can come from other sources
(e.g. audio-buffer-refill windows) independent of this delay-quantum
finding. This number was sitting in this project's very first real-
hardware probe of the night (measuring `delay(1)`'s own granularity) for
hours before anyone connected it to the pacer's actual behavior -- a
reminder that eliminating a hypothesis ("delay(1) isn't BIOS-tick
quantized") doesn't mean the measurement it produced is done being
useful for a *different* question ("does that per-call cost still matter
inside this specific loop structure").

## First framebuffer flush after SDL_Init can spike 50-70ms on 486-class hardware

**A one-time startup cost, not a per-frame one, but big enough to trip an
unrelated safety cap.** dossage/Passage's `DOS_Yield()` cost instrumentation
(`shared/patches/sdl3-dos/0136`, temporary) found steady-state `DOS_Yield()`
cost cheap (~0.24-0.34ms/call), but a sharp spike in the first ~200 frames
on real 486DX2-66 + Cirrus CL-GD5434 hardware -- up to 71ms and 52.6ms,
16 and 12 spikes over 10ms in those two windows only, nowhere else in a
119-second/1651-frame run. Cross-referenced against `SDLDBG.LOG` from the
same run, this lines up precisely with the very first framebuffer flush
(57.65ms by itself -- the first-ever DPMI mapping, first non-bank-0 write)
and three separate `SDL_DOSAudioPump()` "cap tripped (>32 calls in one
sequence)" warnings (`shared/patches/sdl3-dos/0067`'s runaway-caller safety
cap) firing in that same window, right after `OpenDevice`.

**Generalizable, not dossage-specific**: any port using this backend's
frame-limiter pattern (a `SDL_Delay`/`SDL_SYS_DelayNS` wait loop yielding to
the audio thread) plausibly pays a similar one-time cold-start cost around
its first framebuffer flush, and that cost can be large enough to trip
0067's runaway-caller cap and produce a `DOS_Yield()` burst -- worth
knowing before misreading a startup-only warning burst as a steady-state
performance problem. Plausibly concentrated in a title-screen/pre-frame-
counting phase rather than counted gameplay: dossage logged 1700 real-sleep
invocations against only 1651 counted frames, a gap consistent with extra
invocations during a title-wait loop.

**Not fixed, not chased further as of this note** -- flagged as real and
reproducible, not as something needing a shared-layer change yet. If a
future port's startup feels janky specifically in its first second or two
on 486-class hardware, this is a known, already-characterized cause to
check before assuming a new bug.

## Reference pacer patterns from doskutsu (deadline-based, not delta-based)

dossage/Passage's frame-rate investigation (see `docs/optimization.md`)
found real per-frame render work well under budget with no single fixable
bottleneck -- the residual gap traced to the *pacer's own shape*, not to
render cost. doskutsu's `nxengine-evo/src/main.cpp` has two working
pacers worth citing directly rather than reinventing, both confirmed by
reading the actual vendored source, not from memory:

**The render/game-loop pacer** is deadline-based, not delta-based: it
holds an absolute `nexttick` target and compares it against a fresh
`SDL_GetTicks()` read every iteration, rather than computing a sleep
duration once and trusting it. When time remains, it sleeps *half* the
estimated remainder, then loops back and re-measures real elapsed time
against the deadline again -- converging rather than committing to a
single sleep call. `nexttick` resets from the actual current time when a
tick fires (not additively from the previous deadline), so this path
doesn't accumulate unlimited backlog on its own.

**The gameplay simulation pacer**, `run_tick_fixed()`, is a true
fixed-timestep accumulator: an absolute nanosecond timeline
(`SDL_GetTicksNS()`), a fixed tick interval (`TICK_NS = 1e9/GAME_FPS`),
and a catch-up loop that drains accumulated backlog as discrete ticks,
capped at a hard `MAX_CATCHUP` of 5.

**Correction to an earlier version of this note**: MAX_CATCHUP=5 is a
real, validated precedent for a debt ceiling's *size* -- but not because
letting a stall inflate the catch-up backlog is a general gameplay
hazard. It isn't, in doskutsu: outside TAS replay, the accumulator delta
is completely **unclamped** ("bit-for-bit the pre-0281 path" per the
source's own comment) -- a stall can and does inflate `accum_ns`, and the
catch-up loop drains it via up to 5 discrete ticks, unguarded, as normal
shipped gameplay behavior. The stall-discount/clamp machinery (patches
0281/0286/0288) that *does* exist is gated entirely behind
`TAS::active()`/`TAS::replaying()` -- it protects **TAS recorded-input
replay fidelity** specifically (each catch-up tick consumes one recorded
input; during a genuine stall those extra ticks apply the wrong, held
input and drift a recorded route off its path), not general simulation
correctness. Patch 0284's own commit message goes further: 0281's
original premise -- that tick-count inflation itself was what corrupted
replays -- was directly tested and found **wrong** (0283 found an
unrelated input-hold bug was the real cause; a controlled A/B with that
fixed showed clamped and unclamped replays produce identical traces). So
the lesson isn't "a catch-up accumulator needs a stall guard or a loading
screen desyncs the game" -- doskutsu's own gameplay doesn't need one. The
lesson is narrower: **only a port with a discrete-tick-consumes-one-
recorded-input replay/TAS system inherits this specific hazard.** A port
without one (e.g. a continuous-integration pacer where "catching up"
means not sleeping rather than running multiple discrete world-step
calls) has no analogous mechanism to desync in the first place. Take
MAX_CATCHUP's *number* as precedent if useful; don't take the incident
history as evidence that an unclamped catch-up accumulator is dangerous
for gameplay in general -- it demonstrably isn't, in the one shipped
engine this hub has real data on.

**The precedent that matters most across both mechanisms**: doskutsu
paces and measures using SDL's own timer API throughout
(`SDL_GetTicks()`/`SDL_GetTicksNS()`) -- never an engine-private clock
(e.g. a separate `gettimeofday()` call) running in parallel with whatever
clock the delay primitive itself uses. This is the same "single clock
family end-to-end" principle a same-clock instrumentation pass can prove
out before trusting a cross-clock measurement (see the `DOS_Yield()`/
frame-cycle-time diagnostics above) -- and it applies to the *pacing*
decision itself, not just to measuring it: an engine whose own frame-time
measurement and whose delay mechanism read from different clocks has a
second, independent source of per-frame error regardless of which pacer
shape it uses.

## Add debug output for

```
simulation Hz
render FPS
frame time
audio underruns
dropped renders
```

This instrumentation should exist before any optimization work starts —
see `docs/optimization.md`: never optimize without numbers.

**Use `shared/include/runmanifest.h` for the baseline fps/duration/exit
numbers rather than hand-rolling them.** It's a header-only, drop-in
library: include it and call `runmanifest_emit()` once at clean shutdown
for a standardized, greppable `[RUNMANIFEST-BEGIN]`/`[RUNMANIFEST-END]`
block (`fps_p50`, `fps_p95`, `duration_s`, `exit_code`, `environment`, a
build fingerprint, and port-specific extra fields as needed). See
`shared/skills/dos-hardware-validation/references/runmanifest-log.md`.
Wire it in early (STARTS/TITLE_SCREEN milestone), not deferred to
OPTIMIZING — dossage/Passage hand-rolled its own engine-side fps
computation from the start, and a later real-hardware investigation spent
hours chasing an fps gap before finding the actual clock bug lived inside
that same bespoke fps math: the number being measured and the mechanism
under suspicion were the same unreviewed code (see `docs/optimization.md`
for the full investigation). Using the shared, already-reviewed header
doesn't guarantee no bug, but it means the baseline measurement itself
isn't the thing you'd later have to distrust.
