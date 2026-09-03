# DOSSAGE benchmark plan -- 486DX2-66 video-card matrix

Written against the hub's `benchmark` skill. Status: **not yet run.**

## What kind of campaign this is

**This is a characterization campaign, not an optimization campaign.** That
distinction drives everything below and is easy to get wrong.

The fps campaign closed 2026-09-01. The port is at its design ceiling:
`lockedFrameRate = 15` makes the pacer's deadline exactly 66.6667ms and it
advances one tick per frame, so no frame may run fast to repay a slow one and
the measured average can approach 15.000 but never cross it. Steady-state
already sits at **14.9989 fps**.

So the goal here is **not** to make a number bigger. It is to record what
each video card does, with enough rigour that the numbers are comparable and
trustworthy, and to close the one card that has never been verified against
the current build.

Anyone picking this up and starting to "optimize" has misread the situation.

## Section 1 gate: are we compute-bound? (No.)

The `benchmark` skill's first instruction is to confirm you are actually
compute-bound before touching anything. **dossage is not, and this is
already measured** -- do not re-derive it, and do not skip past it:

| | |
|---|---|
| real per-frame work | ~42.7 ms |
| frame budget at 15 fps | 66.67 ms |
| **idle slack** | **~24 ms/frame** |

The port is *pacer-bound*, not compute-bound. `docs/optimization.md`'s
bottleneck list is the wrong tool here; `docs/timing.md`'s pacer material is
the right one. Re-check this if the CPU ever changes -- the regime can shift
between campaigns -- but on this CPU it is settled.

## Pre-campaign work item: the KPI needs percentiles (CLOSED 2026-09-02)

**Resolved.** Option 1 (add RUNMANIFEST percentile emission) was landed as
`patches/passage/0034`, plus a companion `PORT_BUILD_SHA12` Makefile wiring
that closes the separate "nothing in a run log names the build" gap noted
below. `game.cpp` now captures real per-frame duration each loop iteration
and, at the existing clean-exit point, computes `fps_p50`/`fps_p95` and
emits a full RUNMANIFEST block to `RUNMANI.LOG`, alongside (not replacing)
the pre-existing `Frame rate = ...` average printf.

**It did not land clean on the first try, and the failure is itself
informative.** build-qa's DOSBox-X smoke of the first version found
`fps_p50` corrupted to a nonsensical ~13 million in a natural full-length
life -- roughly half of all captured per-frame samples were reading as
near-zero, while the coarse `frameCount/netTime` average stayed a normal
15.02fps throughout. Root cause traced (not fully confirmed by a dedicated
probe) to `Time::getCurrentTime()`'s underlying DJGPP tick counter
occasionally stalling and then "catching up" with a compensating large
delta -- this file's own pacer history already documents that same class of
tick loss elsewhere (`time(NULL)` losing over half its expected ticks in a
reverted busy-wait experiment). The fix filters any captured `frameTime`
that's `<=0` or below `0.001s` (the clock's own documented millisecond
granularity floor) out of the percentile buffer before computation, and
counts rejects visibly (`Frame-time samples: N used, N rejected...`) rather
than silently correcting them away.

**Read this before trusting any dossage `fps_p50`/`fps_p95` figure,
including from real hardware:** under DOSBox-X, the fix's own repeat smoke
run technically passed (both fields present, `fps_p95 <= fps_p50`,
plausible magnitudes) but rejected **60.4%** of samples in a 298s life, and
the surviving ~40% clustered heavily on one or two discrete values (`16.67`
recurring identically across three independent measurements) rather than
spreading naturally. That is consistent with the fix correctly excluding
impossible readings, but it is also consistent with the *surviving* samples
still being coarser/less representative than a genuinely continuous
per-frame signal -- a >60% discard rate is a lot of the distribution to be
reconstructing a percentile from. Per this project's own rule (real
hardware is authoritative, DOSBox-X is an automation gate -- see
`CLAUDE.md`), this was judged not to block moving to real hardware, since
the primary PASS/FAIL band below is average-based and the coarse average is
unaffected by any of this -- but check the real-hardware run's own reject
count and clustering before treating its `fps_p50`/`fps_p95` as more
trustworthy than the average by default. A standalone `Time::getCurrentTime()`
granularity/monotonicity probe (DOSBox-X vs. real hardware) is recommended,
separately, to close this for good -- not done as part of this patch.

## The KPI, written down in advance

Per card, on the 486DX2-66 + PicoGUS rig, build `9c90db0e7905`:

- **PASS:** sustained fps within **0.3 fps** of the card's established figure
  (Cirrus 14.94, ViRGE 14.16-14.5), audio present throughout, no visual
  corruption against `docs/screenshots/dossage-gameplay.png`.
- **FAIL:** outside that band, or audio absent, or corruption.
- **INVALID, not a datum:** hash mismatch on the staged binary; wall-clock
  bracket disagreeing with the engine's own `Game time` by more than the
  established ~14-16 s startup overhead; UniVBE not confirmed active.
- **Mach64 has no established figure** -- see its gate below. Its first run
  is exploratory and cannot pass or fail against a band that does not exist.

Every result is attributed to `build_sha12 = 9c90db0e7905`
(re-baselined 2026-09-02, second time same day; see "The build pin" below).

### The build pin

**`build_sha12 = 9c90db0e7905`**, re-baselined 2026-09-02 (supersedes
`cf5f9a5a861e` from earlier the same day, which was never used to record any
result). Recomputable at any time:

    make build-sha12

It is a content hash of the build's *inputs* -- the three vendor trees
post-patch (folding in each pin and its full applied patch series), the
audio tier, the engine-stage compiler flags, the hub build fragment that
supplies the SDL stage's flags, and the compiler version. Inputs, never the
output: a fingerprint of the binary that is also embedded in the binary
cannot converge. `make game` writes it to `build/dossage.build-sha12` and
`make stage` copies it to `BUILDSHA.TXT` beside `DOSSAGE.EXE`, so it is
readable on the target too.

The corresponding binary is
`sha256 8480da383842a4b273dce263b144fcf0980d839ebcd36126c651126de1775afd`
at `AUDIO_TIER=high` (the default). Independently reproduced by two separate
sessions from a full `make game-clean`, byte-identical both times.

**Known gap CLOSED 2026-09-02.** `patches/passage/0034` wires
`PORT_BUILD_SHA12` into the compile (`Makefile`, `BUILD_SHA12_STAMP` forces
a rebuild when the hash changes, matching the existing `AUDIO_TIER_STAMP`
pattern) and reads it into the RUNMANIFEST block's `binary_sha12` field at
runtime -- confirmed correct via the actual `RUNMANI.LOG` output, not just
the compile line (a `strings`-grep of the binary itself is a false-negative
here: GCC constant-folds the short string into inline stores, invisible to
`strings` despite being genuinely correct -- confirmed via objdump). A rig
log now proves which binary produced it, same as the hub's validation
standard for every other port. `BUILDSHA.TXT` in the staged tree remains a
second, independent witness (catches a staged-tree mixup even before the
binary runs) -- keep hash-verifying it, this doesn't replace that step.

#### Why this was re-baselined, and what it cost

The previous pin, `build_sha12 = 9cace45a`, could not be reproduced or even
recomputed. It appears nowhere else in this repo or the hub, is not a git
commit in either, is 8 hex where the convention is 12, and **no derivation
for it is recorded anywhere** -- so it could be trusted but never checked. A
clean build of the current tree yields `750a5952...`; neither sha1 nor md5
of the binary produces `9cace45a` either. It entered the record in `00a5f72`
(2026-08-31), a planning commit whose own message says the campaign was "Not
yet run" -- an identifier attached retrospectively to an earlier validated
binary.

**This does not impeach the 14.94 fps measurement.**
`.sdl-dos-ports/docs/optimization.md` records the campaign that produced it
in fine detail: steady-state 14.9989 -> 15.089 fps under a tightened pacer
deadline, run average moving 14.936 -> 14.942, stall cost 0.279 -> 0.654
ms/frame, the pacer landing within 0.03 ms of its deadline, and a revert
whose binary was byte-identical to the already-validated build. That is an
internally consistent narrative from a campaign that falsified hypotheses in
turn and published a negative result. The defect was bookkeeping, not
measurement.

**The cost of re-baselining, stated plainly:** the binary that produced
14.94 can no longer be identified, so the PASS bands below (Cirrus 14.94,
ViRGE 14.16-14.5) are **prior-build figures, not same-build comparisons**. A
new number landing outside a band is therefore ambiguous between "this card
regressed" and "this is a different build than the one that set the band."
Treat the first run on the Cirrus as re-establishing the reference under
`cf5f9a5a861e`, not as a pass/fail against 14.94 -- and if it lands within
0.3 fps of 14.94 anyway, that is evidence the two builds are equivalent, not
merely a pass.

**Done, 2026-09-02: 14.817881 fps under `build_sha12=9c90db0e7905`** (which
superseded `cf5f9a5a861e` before this ran -- see "The build pin" above),
0.122 fps from 14.94, inside the 0.3 fps band -- confirms build equivalence
per the paragraph above. Full result: `docs/benchmarks/cirrus-cl-gd5434-2026-09-02.md`.

## The matrix

One CPU (486DX2-66), one sound card (PicoGUS, SB mode), three video cards.
**Not a full cross-product** -- there is no reason to re-sweep CPU or audio.

| Card | State | What this run is for |
|---|---|---|
| Cirrus CL-GD5434 | **re-confirmed 2026-09-02, twice: 14.82 fps (`9c90db0e7905`), 14.77 fps (`f1f867ccadad`)** | Reference/repeatability. Runs banked -- SDL/0019 force-disables LFB for a genuine aperture defect. Two runs, two different builds, both in-band -- see `docs/benchmarks/cirrus-cl-gd5434-2026-09-02.md` and `...-f1f867ccadad.md`. |
| S3 ViRGE 86C375 | validated, 14.16-14.5 fps | Confirm against the current build; earlier figures predate some pacer work. Uses LFB at 320x240x16. |
| **ATI Mach64 215CT/-ET** | **UNVERIFIED** | **Gate first, then measure.** See below. |

### Mach64 gate -- do this before treating any Mach64 number as a datum

Two documented facts collide here, and `HARDWARE.md` has both:

1. **The silicon cannot double-scan.** No 320x200 or 320x240 VESA mode
   exists on this card at all, confirmed via UniVBE's own banner. A
   320x240 request always lands on a larger closest-match mode.
2. **This build compiles in `SDL_HINT_DOS_MAX_BPP=16`** (patch 0029).
   Previous Mach64 testing ran **640x480x24 banked**. The bpp cap changes
   which mode it now negotiates, and that combination has never been run.

So the first Mach64 boot is a **correctness gate, not a benchmark**:
capture the `DOSVESA-MODESET` line and confirm the negotiated geometry/bpp
is sane, then pixel-diff a gameplay capture against the reference screenshot
*before* recording any fps. The Mach64 has a documented history of
real-hardware-only corruption on this port (resolved, port-side, but the
precedent is why the diff comes first).

If it lands somewhere unexpected, that is a finding worth reporting, not an
obstacle to work around.

## Per-card procedure

Same for every card. Batch questions into fewer, longer rounds -- a transfer
round-trip costs a fixed tax regardless of scope.

1. **Operator swaps the card**, then re-runs UniVBE's `UVCONFIG.EXE`
   physically. **Non-negotiable and not automatable.** UniVBE silently
   declines to install for a card it was not configured for; DOS falls back
   to bare ROM VBE, and the failure presents as a crash or wrong mode with
   nothing pointing at the cause. Cost us a round when a Cirrus reported
   VBE 1.2 until UVCONFIG was re-run.
2. **Capture identity witnesses**: CPU class, `oem_string=` (must show a
   VBE 2.0+ provider, not the card's ROM), total VRAM, PicoGUS mode.
   Never assume the rig is what it was last time.
3. **Stage** the whole `build/stage/` tree to a fresh directory. Hash-verify
   before staging *and* after sending. Clear stale logs first -- they append.

   Confirm `./scripts/verify-patches-applied.sh` exits 0 for the build that
   gets staged. Since 2026-09-02 that gate checks series *content*, not just
   patch counts, so it catches a patch file edited in place and never
   re-applied -- which the count-only gate passed while `vendor/` still held
   the old sources. (The older `patches/*-local/` overlay check this step
   used to name is obsolete; the overlay mechanism was retired 2026-09-01
   when patches became per-port vendored. The hazard it guarded against is
   unchanged: a forgotten diagnostic costs real fps with nothing in the run
   output to flag it -- dossage's own migration test used a 0.70ms/frame
   one, which alone would have moved every number in this campaign.)
4. **Run** >= 3 minutes of gameplay so startup amortizes (the first ~300
   frames are genuinely slower; a short run reports startup, not steady
   state). Fold `vcctrl_audio_verdict` into mid-run health checks, not just
   video.
5. **Report raw**: the exit line verbatim, `SDLDBG.LOG` mode-set lines, a
   gameplay capture, **and the operator's own wall-clock bracket, unprompted**
   -- the engine's `Game time` divides by `time(NULL)`, which the game itself
   can corrupt. That is not hypothetical: a fix once made the *reported* fps
   improve to 14.3 while the real rate collapsed to ~6.5, and only the
   independent bracket caught it.
6. **Record** using `.sdl-dos-ports/templates/BENCHMARK.md`, one file per run
   under `docs/benchmarks/`.

## Team shape

Two peer sessions, per `shared/agents/README.md`: one investigating session
and one dedicated rig operator. Not solo -- this is iterative real-hardware
work across three hardware swaps.

**Write a rig runbook first, in this repo, before starting.** During the
closing campaign the rig-operator session was `/clear`ed mid-campaign and
every piece of procedural knowledge vanished instantly -- how to stage, how
to launch, the practices it had developed -- while `PLAN.md` and the hub docs
retained every *finding*. It had to be re-briefed from zero. Vendoring
vcctrl's skills into `.agents/` (done) covers the general mechanics; the
port-specific runbook does not exist yet and should.

## Existing reference data

`PLAN.md`'s measured-constants table holds every real-hardware constant from
the closing campaign -- `delay(1)` granularity, `DOS_Yield` cost, per-frame
work, the VRAM-flush stall magnitudes. **Check a new hypothesis against that
table before spending rig time on it.** That table exists because the very
first probe of the closing campaign measured the number that turned out to be
the root cause, and it sat unrecognised in a log for hours.

## Known-open items this campaign may touch

- **The banked-flush stall.** The VRAM flush is normally ~2.2 ms and spikes
  to 243-326 ms, 3-4 times per run. Localised to `WaitForVBlank()` running on
  every flush (vsync unconditionally armed); one variable left -- the
  guard-loop iteration count on stall vs. normal frames. Shared-layer work,
  not dossage's. If a card shows a materially different stall profile, that
  is real evidence about the mechanism and worth capturing.
- **Post-exit black screen.** ~20 s after ESC, PS/2 responsive throughout,
  reboot normal -- so not a hang. Looks like video mode teardown on exit.
  Unrelated to fps; note it if it recurs per-card. **Recurred on the Cirrus
  leg, 2026-09-02** (capture stick briefly lost lock, resolved via the
  hardware camera to a clean prompt) -- second confirmed occurrence.
- **`fps_p50`/`fps_p95` measure the wrong window -- RESOLVED (explained,
  fix not yet landed) 2026-09-02.** Both fields still cluster above the
  15fps design ceiling on real hardware (e.g. `fps_p50=fps_p95=16.67` in
  the Cirrus run) even after the reject-filter fix. **Not a clock bug --
  a definitional one, root-caused from the actual code, not measurement
  noise:** `patches/passage/0034` captures `frameTime` as
  `newTimestamp - lastFrameTimeStamp`, a window that starts *after* the
  previous iteration's pacer sleep and ends *before* the current
  iteration's own sleep -- i.e. it measures pure work time (render +
  game logic + audio pump), deliberately excluding every frame's sleep,
  because that is what the pacer's own `extraTime` calculation has
  always needed it for. Since the pacer pads work time up to the full
  66.67ms budget with sleep, work time is structurally never bounded by
  15fps -- 1/work-time is >= 15fps whenever there is any slack at all,
  which this rig has by ~24ms/frame (Section 1 gate, above). 60ms of
  real work -> 16.67fps is squarely inside that documented range, not an
  anomaly.
  (Earlier text in this section guessed a DJGPP/DOSBox-X clock-
  granularity explanation instead -- that was wrong, corrected here per
  this project's own "correct the record" discipline. The lower reject
  rate on real hardware, 24.0% vs. DOSBox-X's 60.4%, is real and still
  stands as its own separate, true observation -- it just isn't the
  explanation for the clustering.)
  **The actual fix, scoped but not yet implemented**: measure the *full
  paced period* instead -- the delta between consecutive
  `lastFrameTimeStamp` values (both taken after their own sleep), not
  `newTimestamp - lastFrameTimeStamp`. That window sits at ~66.67ms in
  steady state (bounding instantaneous fps at <=15, matching the design
  ceiling) and only exceeds it on a genuine stall -- the actual "5% of
  frames were slower" shape a stall-catching KPI needs, and what the
  original motivating story (a hidden low p95 next to a 14.94 average)
  was always about. Queued as a new patch slot (`0035` at the DJGPP-
  patch series' current tip) rather than amended into `0034` in place,
  since `0034`'s specific diff already has a real-hardware PASS result
  attached to it
  (`docs/benchmarks/cirrus-cl-gd5434-2026-09-02.md`) -- freezing that
  provenance outweighs keeping the patch count minimal, and it's a
  distinct concern per this repo's own patch conventions regardless.
  **Landed, `build_sha12=f1f867ccadad`, confirmed on real hardware
  2026-09-02: real, not a DOSBox-X artifact.** `fps_p50=16.67` (~60ms)
  and `fps_p95=9.09` (~110ms) reproduce identically on real hardware and
  DOSBox-X, both now with a near-zero reject rate (real hardware: 0.22%;
  DOSBox-X: 0.02% -- both essentially measuring the complete real
  distribution, not a filtered subset). This rules out the DOSBox-X-
  timer-artifact hypothesis raised earlier the same day -- per this
  project's own rule (real hardware authoritative, DOSBox-X an
  automation gate, `CLAUDE.md`), that hypothesis is now falsified, not
  merely undecided. **This port's per-frame paced-period timing is
  genuinely bimodal** on this CPU tier -- two real, environment-
  independent clusters, not a single distribution around the 66.67ms
  budget as assumed. Working (unconfirmed) explanation: 110ms is close
  to 2x the classic PC BIOS/PIT tick (~54.925ms, 18.2Hz -- 9.09fps from
  109.85ms is a near-exact match); 60ms doesn't cleanly match a small
  integer multiple of that same tick, source not yet identified.
  `fps_p50`/`fps_p95` are now **trustworthy as measurements** (the
  capture is accurate on both environments) but the *interpretation*
  needs care: this is a genuine bimodal-distribution finding, not the
  single-tail-stall shape the KPI was originally written to expose --
  don't compare it across cards as a simple number yet. **Queued as a
  standalone `Time::getCurrentTime()`/pacer-timing probe** (isolated
  from the full game) to identify the 60ms cluster's source -- two
  independent sessions (build-qa, vcctrl-c3) both converged on
  recommending this rather than guessing further from full-game data.
  Not performed as part of this campaign. Full data:
  `docs/benchmarks/cirrus-cl-gd5434-2026-09-02-f1f867ccadad.md`.
