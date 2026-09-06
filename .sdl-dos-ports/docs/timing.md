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

**`gettimeofday()` has no sub-second resolution of its own, so a period
measured with it collapses onto a few discrete values.** DJGPP's
`gettimeofday()` fills `tv_usec` from DOS's own clock (INT 21h AH=2Ch,
hour:minute:second:hundredths -- the `AH=0x2C` call is right there in
`gettimeo.o` inside the toolchain's `libc.a`), and DJGPP's libc reference
says so outright: "precise to less than 1/20 of a second only ... the
underlying DOS function has 1/20 second granularity, as it is calculated
from the 55 ms timer tick count". DOS advances its hundredths by an uneven
mix of steps per 18.2065 Hz tick (100 does not divide by 18.2065), so a
genuinely steady period measured this way comes out as whichever whole
number of ticks elapsed during that frame's phase against the tick
boundary, rounded to a hundredth. dossage/Passage saw exactly this
(`docs/PACER-TIMING-INVESTIGATION.md` there, resolved 2026-09-03): a
deadline pacer converging on a true ~66.67 ms period against
`SDL_GetTicksNS()`, measured per frame with `gettimeofday()`, produced
`fps_p50`/`fps_p95` of exactly 16.67/9.09 on every CPU and card, and every
one of 256 raw samples checked was exactly 50, 60, 100, 110 or 330 ms --
reproduced in complete isolation by a standalone probe with no SDL,
audio, or rendering, so it is hardware-independent by construction.
Never use `gettimeofday()`, or anything built on it such as an engine's
own portable "current time" wrapper, for sub-second frame-timing
instrumentation on this platform; use `SDL_GetTicksNS()` / `uclock()`,
the same clock family the pacer runs on. This is a second, independently
confirmed root cause behind the "single clock family end-to-end" rule in
the pacer-patterns section below.

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

## RDTSC from real-mode code hangs the g2k Pentium OverDrive under EMM386 (DJGPP protected mode: not affected so far)

**Finding (2026-09-03, vcctrl rig).** With the Pentium OverDrive 83
installed, the first `RDTSC` executed by a *real-mode* DOS program running
as a V86 task under MS-DOS 6.22 `EMM386 NOEMS` never returns. Interrupts
keep being serviced (keyboard LEDs still respond) but there is no forward
progress; Enter, Ctrl-Break and Ctrl-Alt-Del do nothing, and only a power
cycle recovers. Reproduced twice in `dinspect` (the sibling 16-bit Open
Watcom hardware-inventory tool, via a step-by-step trace build that left
the line printed immediately before `RDTSC` as the last thing on screen)
and independently by HWiNFO for DOS the same day. `CPUID` under the same
EMM386 is fine. Every boot profile on that rig loads EMM386 in `[COMMON]`,
so there is no EMM386-free profile to fall back to. The mechanism has not
been identified (EMM386 reflecting an opcode it cannot emulate, or
something specific to the OverDrive); do not repeat a guess as if it were
known.

**Why every 486-class result was silent about this.** The OverDrive is the
only CPU in the rig's matrix with a time-stamp counter at all. Code that
gates `RDTSC` on `CPUID`'s TSC bit passes every 486DX2 / Am5x86 test by
never executing the instruction, then wedges on the first Pentium-class
tier.

**What it does and does not cover.**

- *Real-mode / V86 code* (a 16-bit Open Watcom or Turbo C utility, a setup
  tool, anything launched from a BAT that is not a DPMI program): treat
  `RDTSC` as unsafe under a V86 monitor. dinspect's fix is the right one
  there: read CR0 via `SMSW` (unprivileged, never traps); if PE is set
  while a "real-mode" program is running, a V86 monitor (EMM386, QEMM, a
  Windows DOS box) is underneath -- skip `RDTSC` and use a PIT-timed loop.
- *DJGPP protected-mode code* (every port built on this hub, every DJGPP
  probe): the same rig, CPU and EMM386 line has been executing `RDTSC`
  from a CWSDPMI DPMI client without incident. doskutsu's 2026-08-13
  round-2 QA matrix on the OverDrive (`qa-results/2026-08-13-r2-POD83/
  *SDL.LOG`; UNIVBE boot profile with `EMM386 NOEMS`; cpu-witness family 5
  model 3 stepping 2) logs `audio IRQ timer: RDTSC (Pentium-class
  detected, 83 cycles/us ~= 83 MHz)` on every cell: SDL/0039 ran a 10 ms
  `RDTSC` calibration spin at device open and then executed `RDTSC` inside
  every Sound Blaster IRQ for whole sessions. A ring-3 DPMI client under
  CWSDPMI (hosted via VCPI, with its own GDT/IDT) is a different execution
  context from a V86 task, and it is the context a port actually runs in.
  Treat this as "observed not to hang", not "proven safe".

**What a DJGPP program can and cannot detect.** The `SMSW`/PE test is
meaningless from a DPMI client: PE is always set in protected mode, so it
would disable `RDTSC` everywhere, DOSBox-X and EMM386-free machines
included. DPMI function 0400h's flags bit 1 ("host returns to real mode,
not V86, for reflected interrupts") is no better: CWSDPMI hardcodes the
flags word to 1 or 5 (`exphdlr.c`, `case 0x0400: tss_ebx =
dalloc_max_size() ? 5 : 1`), so it reports V86 whether or not a monitor is
loaded. The only real signal is VCPI presence (INT 67h AX=DE00h), which
the shared layer deliberately does not consult because the evidence above
says its context is fine.

**What the shared layer does.** `shared/patches/sdl3-dos/0039` keeps its
default (RDTSC when `CPUID` reports a TSC, 8253 PIT counter 0 otherwise).
`0138` adds a runtime killswitch, `SDL_HINT_DOS_AUDIO_TIMER_RDTSC=0`
(strict `0`), that selects the PIT path unconditionally and logs
`audio IRQ timer: 8253 PIT counter 0 (RDTSC disabled via
SDL_HINT_DOS_AUDIO_TIMER_RDTSC=0; ...)`. If a run on new Pentium-class
hardware, or under a different memory manager, stops at audio device open
with the symptoms above, set that env var before a rebuild is even
considered. The PIT path costs only telemetry resolution (~838 ns per
tick against ~12 ns) and is the path every 486-class result on this hub
was measured with.

**Real-hardware A/B of the killswitch: pending.** The OverDrive was
swapped out of the rig for a 486DX2-50 campaign on 2026-09-03 before the
`0138` build could be run on it, and a 486 predates `CPUID`, so nothing
RDTSC-related can be tested until a Pentium-class CPU is back in the
socket. The witness is `shared/tests/probes/audtimer.c` + `audtimer.bat`
(SDL3-linked; step 1 forces the killswitch and never executes `RDTSC`,
step 2 is the production default). Expected on the OverDrive: step 1
logs `8253 PIT counter 0 (RDTSC disabled via
SDL_HINT_DOS_AUDIO_TIMER_RDTSC=0; ...)`, step 2 logs `RDTSC
(Pentium-class detected, 83 cycles/us ~= 83 MHz)`. Whoever runs it next:
record the result here and in `HARDWARE.md`'s row.

**For port-side and probe code.** Prefer `uclock()` / the PIT for timing.
If a port or probe wants `RDTSC` from DJGPP, gate it on `CPUID` (TSC bit),
give it a killswitch, and put the first `RDTSC` somewhere a wedge would be
recognized immediately, not inside an IRQ handler. Never execute it from
any real-mode helper without the `SMSW` check.

## Open (dossage, 2026-09-05): a fixed-length run can land one second over an integer-second boundary on some CPU/card pairings and not others

Unconfirmed and not chased -- recorded so the next port doing precision
fps work recognizes the shape. Across dossage/Passage's full 3-card x
4-CPU matrix on one build, a natural ~298 s life measured as 299 s on
some pairings (Cirrus at both 486DX2 tiers, Mach64 on Am5x86, and Mach64
on 486DX2-50 on two of three repeat lives) and 298 s on the rest, with
S3 ViRGE never showing it at any tier. Neither banked-vs-LFB, framebuffer
bytes per frame, nor CPU tier explains the table on its own. The working
theory in `docs/benchmarks/mach64-215ct-486dx2-66-2026-09-05.md` there:
overall per-frame margin sets how close a pairing sits to the rounding
boundary, and ordinary run-to-run jitter (plausibly the same
DOS-tick-phase quantization as the `gettimeofday()` finding above)
decides which side an individual life lands on. Separating those two
needs repeat lives per pairing or per-frame-cost instrumentation;
nothing in that table threatened a KPI, so it stayed open.

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
(e.g. a separate `gettimeofday()` call -- which on DJGPP has no
sub-second resolution of its own anyway, see "Your clock can lie")
running in parallel with whatever clock the delay primitive itself uses. This is the same "single clock
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
