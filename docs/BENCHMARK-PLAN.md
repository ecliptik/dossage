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

## Pre-campaign work item: the KPI needs percentiles

**dossage currently cannot express the KPI the skill requires.** It reports
only Passage's own single average (`frameCount / netTime` at `game.cpp`), and
emits no `fps_p50`/`fps_p95` via `shared/include/runmanifest.h`.

That is not a formality. The whole difficulty of the closing campaign was
that a single average is dominated by a handful of ~250-325 ms stalls: the
average read 14.94 while steady-state was 14.9989. **A p50 would have shown
~15.0 next to a much lower p95 on the very first run, and the stall story
would have been obvious immediately instead of after eight falsified
hypotheses.**

Two options, decide before running:

1. **Add RUNMANIFEST percentile emission** (preferred). Non-trivial -- needs
   per-frame time capture plus a percentile computation at exit. Landing it
   as a port-local patch is the natural home.
2. **Run with the single average and document the limitation explicitly** in
   every recorded result, so nobody later compares a dossage average against
   another port's p50 as though they were the same measurement.

Do not silently do (2) while writing the result up as though it satisfied the
KPI.

## The KPI, written down in advance

Per card, on the 486DX2-66 + PicoGUS rig, build `cf5f9a5a861e`:

- **PASS:** sustained fps within **0.3 fps** of the card's established figure
  (Cirrus 14.94, ViRGE 14.16-14.5), audio present throughout, no visual
  corruption against `docs/screenshots/dossage-gameplay.png`.
- **FAIL:** outside that band, or audio absent, or corruption.
- **INVALID, not a datum:** hash mismatch on the staged binary; wall-clock
  bracket disagreeing with the engine's own `Game time` by more than the
  established ~14-16 s startup overhead; UniVBE not confirmed active.
- **Mach64 has no established figure** -- see its gate below. Its first run
  is exploratory and cannot pass or fail against a band that does not exist.

Every result is attributed to `build_sha12 = cf5f9a5a861e`
(re-baselined 2026-09-02; see "The build pin" below).

### The build pin

**`build_sha12 = cf5f9a5a861e`**, re-baselined 2026-09-02. Recomputable at
any time:

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
`sha256 750a5952777c3a3f06debaaaae4d4c3601df8d503257752244f9d9599aed9a74`
at `AUDIO_TIER=high` (the default). Verified deterministic: two independent
builds from a full `make game-clean` produced byte-identical output.

**Known gap -- nothing in a run log names the build.** The hub's validation
standard has the binary emit `build_sha12` into its own runmanifest, so a
rig log proves which binary produced it. dossage has no runmanifest emission
at all, so there is nothing to print the field; passing
`-DPORT_BUILD_SHA12=` today would compile a macro no code reads. Closing
this properly needs an engine-side change under `patches/passage/`, which
would itself alter the binary -- so it must land *before* a baseline, never
between a baseline and the run it anchors. Until then `BUILDSHA.TXT` in the
staged tree is the witness, and the pre-flight's staged-tree hash check is
what actually guards against running the wrong build.

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

## The matrix

One CPU (486DX2-66), one sound card (PicoGUS, SB mode), three video cards.
**Not a full cross-product** -- there is no reason to re-sweep CPU or audio.

| Card | State | What this run is for |
|---|---|---|
| Cirrus CL-GD5430 | validated, 14.94 fps | Reference/repeatability. Runs banked -- SDL/0019 force-disables LFB for a genuine aperture defect. |
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
  Unrelated to fps; note it if it recurs per-card.
