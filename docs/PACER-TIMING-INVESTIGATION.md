# Pacer-timing investigation: the bimodal fps_p50/fps_p95 finding

Written 2026-09-03 as a standalone brief for whoever (probe-engineer,
another session) picks this up next. This is deliberately self-contained
-- read this file and its cited sources, you should not need to re-derive
anything already established here or ask the benchmark campaign session
for more context. If something below turns out wrong, correct it in this
file and say so explicitly (see `docs/BENCHMARK-PLAN.md`'s own "correct
the record" discipline) -- don't silently work around a stale claim.

## The question

Since patch `patches/passage/0035` started measuring the real paced
period (not just work-time -- see that patch's own commit message),
every real-hardware run this campaign has produced **exactly the same
two values**: `fps_p50=16.67` (~60ms) and `fps_p95=9.09` (~110ms),
regardless of:

- Video card (Cirrus CL-GD5434 banked, S3 ViRGE LFB, ATI Mach64 LFB --
  three different chip vendors, two different framebuffer-access paths)
- CPU clock speed AND vendor (486DX2-66, 486DX2-50 -- Intel, ~24% clock
  difference from each other -- and Am5x86-133, AMD)

**Why this is surprising, precisely**: the *plain* average fps (computed
via `time(NULL)`, structurally different from the percentile capture)
spans a full 13.69-15.02fps range across the three CPU results -- so the
game's real per-frame behavior IS CPU-dependent, as expected. But the two
specific percentile values did not move AT ALL, on any of the five runs
across five independent axes. That's the anomaly: whatever produces
exactly 60ms and exactly 110ms as the two dominant clusters in the
paced-period distribution is apparently indifferent to video hardware,
CPU clock speed, and CPU vendor -- which points at something running on
a fixed, independent clock source, not at anything workload- or
CPU-cycle-driven.

**Not yet answered**: what specifically produces these two values, and
why exactly 60ms and 110ms (not, say, 66.67ms +/- jitter, which is what
the pacer is actually trying to deliver).

## The evidence, compactly

| Run | CPU | Card | Avg FPS | fps_p50 | fps_p95 | Reject rate |
|---|---|---|---:|---:|---:|---:|
| `cirrus-cl-gd5434-2026-09-02-f1f867ccadad.md` | 486DX2-66 | Cirrus (banked) | 14.769 | 16.67 | 9.09 | 0.27% |
| `virge-86c375-2026-09-03.md` | 486DX2-66 | ViRGE (LFB) | 14.818 | 16.67 | 9.09 | 0.27% |
| `mach64-215ct-2026-09-03.md` | 486DX2-66 | Mach64 (LFB) | 14.769 | 16.67 | 9.09 | 0.27% |
| `mach64-215ct-486dx2-50-2026-09-03.md` | 486DX2-50 | Mach64 (LFB) | 13.685 | 16.67 | 9.09 | **0.00%** |
| `mach64-215ct-am5x86-2026-09-03.md` | Am5x86-133 (AMD) | Mach64 (LFB) | 15.017 | 16.67 | 9.09 | 0.29% |

All five files are in `docs/benchmarks/`. Each has a `Notes` section with
the specific reasoning for that run; this document doesn't repeat it,
only the cross-run pattern. Note the Am5x86 row's CPU identity carries
its own open caveat (dinspect misdetected the FPU as absent, confirmed
wrong by a functional test; the ~100MHz-vs-133MHz speed question is
unresolved) -- see that file's Hardware section, not itself relevant to
the pacer-timing question but worth knowing before citing that run's
average elsewhere.

Reject rate is the fraction of captured paced-period samples patch
`0035`'s filter excluded as implausible (`<=0` or `<0.001s` --
see that patch for why). It fell to **exactly zero** at 486DX2-50, the
cleanest of any run -- worth investigating whether that's coincidence or
itself informative (a slower CPU never produces a frame fast enough to
trigger whatever causes a near-zero/negative reading elsewhere).

## Why this is NOT already answered by existing work

This repo already has a purpose-built timing-probe suite --
`tests/probes/{dlygran,clkdrift,clkscale}.c` -- from an earlier
investigation (2026-09-01, the pacer-tightening campaign chasing the
15.000-vs-14.9989fps gap, see `PLAN.md`'s "measured constants" table and
surrounding history). **Read `tests/probes/README.md` before doing
anything else** -- it documents each probe's purpose, method, and
real-hardware results in detail. Do not rebuild any of this from
scratch; extend or reuse it.

What those probes already established, on real hardware, 486DX2-66 only:

| Probe | Finding | Status |
|---|---|---|
| `dlygran` | DJGPP `delay(1)` is fine-grained: mean 1.236ms, p95 1.244ms -- NOT BIOS-tick-coarse (~54.9ms) | Closed |
| `clkdrift` | `gettimeofday()` (what `Time::getCurrentTime()` uses) and `uclock()` (what `SDL_Delay()`'s wait loop uses) tick at consistent relative rates | Closed |
| `clkscale` | `uclock()`, `time(NULL)`, and the CMOS RTC (an independent 32.768kHz-crystal clock) agree to within **0.03%** -- ruled out a clock-rate-mismatch explanation for the game's own small (~1.72%, ~67.8ms-vs-66.67ms) fps-report gap | Closed |

**None of these explain today's finding**, and it's important to see
precisely why not, not just assume more probing is needed:

1. **Different order of magnitude and different shape.** The prior work
   found a *smooth, ~1.7%, constant scale-factor* effect (a slightly
   miscalibrated ruler) and *fine-grained, ~1ms-precision* timing
   (`delay(1)`, `uclock()`). Today's finding is a *sharp, two-value,
   ~45%-magnitude* split (60ms vs. 110ms is a factor of ~1.83x) in
   individual paced-period samples. A 1ms-precision, 1.7%-scale-error
   world does not predict a clean bimodal split this large.
2. **Different measurement site.** The existing probes measure the
   clocks in isolation (a tight standalone loop, no SDL, no rendering,
   no audio, no DPMI paging beyond the probe's own tiny footprint).
   Patch `0035`'s capture happens inside the *full game loop*, after a
   real render, audio-pump, and event-poll each iteration. Something
   about that fuller context could matter and none of the existing
   probes would have seen it.
3. **Never run at any CPU speed other than 486DX2-66.** All three
   probes' real-hardware numbers above are 66MHz-only. Today's
   CPU-speed-independence finding for `fps_p50`/`fps_p95` has no
   probe-level counterpart to compare against yet -- it's possible
   `dlygran`'s "1.236ms, fine-grained" result *itself* changes at
   486DX2-50 (in which case the story is more complex than "it's a
   fixed hardware tick"), or stays put (which would strengthen the
   hardware-timer hypothesis further). Nobody has checked.
4. **`clkdrift` may already contain the answer, unexamined.** `clkdrift`
   already collects repeated `gettimeofday()`-vs-`uclock()` samples
   across "several known interval lengths" (`tests/probes/README.md`)
   and reports whether they *agree on average* -- but if its raw
   per-sample data (not just the aggregate verdict) was ever discarded
   rather than inspected for distribution *shape*, a bimodal pattern
   sitting right there could have been missed the same way the original
   "1.236ms quantum" constant sat unrecognised in a log for hours before
   someone cross-referenced it (see `PLAN.md`'s own retrospective on
   that). **Check this first, it's nearly free** -- see Action 1 below.

## Working hypothesis, still unconfirmed

110ms is close to 2x the classic PC BIOS/PIT tick period (65536/1193182
Hz = 54.9255ms; 2x = 109.85ms, vs. the observed ~110ms/9.09fps -- a
close match). 60ms does not cleanly match any small integer multiple of
that same tick (60/54.9255 = 1.093, not near an integer), so if the
mechanism really is tick-based, 60ms likely has a different source, or
the tick-multiple theory is wrong for at least one of the two clusters.
**Do not treat this as confirmed** -- it is pattern-matching on two
numbers, offered by the vcctrl session that ran the real-hardware
legs, explicitly flagged there as "not my diagnosis to make." Verify or
falsify it with actual data, don't just repeat it as fact.

## Suggested actions, in priority order (cheapest/highest-value first)

1. **Re-examine (or re-run and this time keep) `clkdrift`'s raw
   per-sample data.** Don't just check whether the aggregate rate
   agrees -- histogram the actual sample-to-sample deltas at each
   interval length and look for a bimodal or multi-modal shape. If
   `clkdrift.c` doesn't currently print per-sample raw values (only a
   summary verdict), that's a small, low-risk change to make before
   re-running it. This could plausibly answer the question with zero
   new probe design, just a different look at the existing tool.
2. **Write a minimal probe that reproduces patch `0035`'s exact capture
   pattern**, isolated from the full game: a loop that does a fixed
   amount of "work" (or none) then sleeps to hit a target
   ~66.67ms/iteration period via the same `SDL_Delay()` mechanism the
   real pacer uses (or, if that's not practical standalone, DJGPP's
   `delay()`/`uclock()` equivalent -- but match the real call pattern
   as closely as possible, since item 3 above is exactly the risk of
   not doing so), capturing `Time::getCurrentTime()` deltas exactly as
   `game.cpp` does post-`0035`, for a few thousand iterations. Compare
   its distribution shape against the four real-game runs' data above.
   If the bimodal split reproduces in this isolated probe, that's very
   strong evidence the cause is NOT anything full-game-specific
   (rendering, audio pump, event polling) -- if it does NOT reproduce,
   that redirects the investigation toward what's different about the
   full game loop.
3. **Run `dlygran` and `clkscale` on 486DX2-50** (and ideally every
   future CPU tier this campaign visits) -- cheap, already-built, and
   directly tests whether the *other* timing constants this port
   depends on are also CPU-speed-invariant, or whether `fps_p50`/
   `fps_p95` are uniquely so. Either answer is informative.
4. Once a mechanism is identified (or ruled out down to a short list),
   write up the finding in `docs/BENCHMARK-PLAN.md`'s Known-open-items
   section (that's where all four runs above are currently cross-linked
   from) and in `PLAN.md`'s measured-constants table if it resolves to
   a durable constant worth recording there, following that table's
   existing format.

## What this does NOT need

- Does not need real-hardware rig time to *start* -- action 1 needs only
  a rebuild if per-sample logging isn't already there, then a real-hw
  run once ready; action 2 is pure DJGPP probe authoring (build/DOSBox-X
  smoke locally first, per `probe-engineer`'s own charter, before asking
  for real-hardware time).
- Does not block the ongoing CPU/video-card benchmark sweep -- that
  continues in parallel via the `vcctrl-c3` peer session. Coordinate
  real-hardware rig time with whichever session is driving that sweep
  when a probe is ready to run for real, rather than assuming the rig is
  free.
- Is not an optimization task. `docs/BENCHMARK-PLAN.md`'s own framing
  applies here too: this is characterization. A stall-catching KPI needs
  to be trustworthy; making the game faster is explicitly out of scope
  for this campaign.
