# DOSSAGE benchmark plan -- 486DX2-66 video-card matrix

**Current status, 2026-09-05: full 3-card x 4-CPU matrix (Mach64,
ViRGE, Cirrus x 486DX2-50, 486DX2-66, Am5x86-133, Pentium OverDrive 83
-- twelve pairings) is measured on the current merged fix build
(`build_sha12=a5e9835f12e7`) and every cell PASSES.** This closed in
stages through 2026-09-04/05 -- see the dated entries below for how
each sweep went and the "one second over" characterization finding
that came out of closing the last few gaps. Nothing left queued in
this matrix as of this writing.

Written against the hub's `benchmark` skill. Status: **486DX2-66 video-card
matrix complete (2026-09-03)** -- Cirrus, ViRGE, and Mach64 all measured.
**Scope expanded 2026-09-03, operator-directed, into a CPU dimension** this
doc originally declined to cover (see "The matrix" below and the CPU-tier
table added there) -- 486DX2-50 + Mach64 is the first cell of that
expansion. **486DX2-50's own three-round fix-validation arc (pacer-timing
+ audio-tier fixes, then Phase 2 optimization) is now CLOSED, 2026-09-04:
KPI met, `patches/passage/0036`/`0038`-`0041` land the DX2-50, the port's
own minimum-target hardware, at a solid 14.99fps average.** See "The
matrix" below and Rounds 1-3
(`docs/benchmarks/mach64-215ct-486dx2-50-round{1,2,3}-2026-09-04.md`).

**`dx2-50-15fps` MERGED to `main` 2026-09-04 as `2881cfc`** (no-ff, the
user's explicit "Yes merge"; not pushed to the remote). All fixes referenced
below as "not yet landed"/"in progress" earlier the same day are now on
`main` -- read those mentions as historical. Raw rig logs for the campaign
moved out of the `dossage-dx2-50` worktree into this repo's own untracked
`rawlogs/dx2-50-campaign-2026-09/` (see `rawlogs/README.md`); the round
records below cite that path now, not the worktree.

**Mach64: all four CPU tiers have a datum, but not all on the current
build -- see the correction dated 2026-09-05 further down and in the
Mach64 CPU-tier expansion section.** The tier that needed a
fix-validation arc (486DX2-50, the port's own minimum target) closed at
KPI PASS on the fix. Am5x86-133 and POD83 are still on the pre-merge
build; not queued as of this writing, but not "done" in the same sense
ViRGE/Cirrus below are.

**ViRGE CPU-tier sweep CLOSED, 2026-09-04.** All four CPU tiers
(486DX2-50, 486DX2-66, Am5x86-133, Pentium OverDrive 83) now have a
ViRGE datum on the merged fix build (`build_sha12=a5e9835f12e7`),
matching the Mach64 sweep's coverage -- **every leg PASSED**, and every
CPU with pacer slack (all but 486DX2-50) landed on the identical
15.016779fps/4475-frame figure, five independent CPU/card combinations
total now confirming that's the design's deterministic ceiling-hit
signature. The 486DX2-66 run also closed a real gap: the original
486DX2-66+ViRGE datum predated the audio-tier fix and was never
independently re-measured against it -- now it has been, clean. See
"The matrix" below (ViRGE CPU-tier expansion) and
`docs/benchmarks/virge-86c375-{486dx2-50,486dx2-66,am5x86,pod83}-2026-09-04.md`.
Nothing left queued for the ViRGE sweep.

**Cirrus CPU-tier sweep started 2026-09-04.** First cell (Pentium
OverDrive 83 -- also the first Cirrus datum on the merged build)
**PASS, 15.016779fps**, `build_sha12=a5e9835f12e7` -- the sixth
appearance of that same fps/frame-count signature and the first on a
banked (not LFB) card. Hit and recovered from the same
UVCONFIG-not-yet-run gap the original ViRGE swap did. See "The matrix"
below (Cirrus CPU-tier expansion) and
`docs/benchmarks/cirrus-cl-gd5434-pod83-2026-09-04.md`.

**Second cell, 2026-09-05, found a real exception, corrected in place
rather than left as the overclaim it briefly was.** The POD83 result
above was read (in this banner and that file's own writeup) as proving
the 15.016779fps signature "video-path-independent as well as
CPU-tier-independent" -- wrong, not merely premature. Cirrus (banked) +
486DX2-66, re-confirmed on the merged build (PASS, KPI-clearing), landed
on **14.966555fps (299s, not 298s, same 4475 frames)**, reproduced
identically across two independent lives, not scatter. Working
explanation: banked's known extra per-frame cost vs. LFB is small
enough to be absorbed by POD83's headroom but apparently just enough to
tip 486DX2-66's tighter margin over the 298/299-second rounding
boundary -- two orders of magnitude smaller than the 486DX2-50 tier's
compute-bound story, doesn't threaten the KPI, but breaks the
otherwise-universal signature. See "The matrix" below and
`docs/benchmarks/cirrus-cl-gd5434-486dx2-66-2026-09-05.md`.

**Third cell, same day: 486DX2-50, the port's own minimum-target CPU,
PASS at 14.966555fps -- and the banked-path effect did not scale up at
the tighter margin as feared.** Identical 299s/4475-frame shape to the
486DX2-66 result, exactly the same magnitude rather than a worse one.
Completes KPI confirmation on all three supported video cards at this
CPU tier (Mach64 Round 3, ViRGE, now Cirrus). See
`docs/benchmarks/cirrus-cl-gd5434-486dx2-50-2026-09-05.md`.

**Fourth and closing cell, same day: Am5x86-133, PASS at the usual
15.016779fps (298s).** Both 486DX2 tiers show the 299s "one second
over" effect; both faster tiers (Am5x86-133, this cell, and POD83, the
sweep's opening cell) land exactly on the campaign's usual signature
with no measurable video-path penalty at all *within Cirrus*. **This
read as a clean CPU-tier boundary at the time -- revised the very next
day, see the 2026-09-05 Mach64+Am5x86-133 entry above and in the Mach64
section below: this same CPU tier (Am5x86-133) shows the "one second
over" effect on Mach64, meaning it is not a CPU-tier boundary after
all.** Left as it was written below, correction stated once here rather
than rewritten throughout. **Closes the Cirrus CPU-tier
sweep: all four CPU tiers now have a Cirrus datum, every one PASS.**
See `docs/benchmarks/cirrus-cl-gd5434-am5x86-2026-09-05.md`. Nothing
left queued for Cirrus.

**Correcting a claim in the process of writing this**: ViRGE and Cirrus
now genuinely have all four CPUs measured *on the merged fix build*
(`a5e9835f12e7`). Mach64 does not, despite this doc's own earlier
"nothing queued for Mach64" language -- its Am5x86-133 and POD83 rows
below are both still `build_sha12=f1f867ccadad`, which predates the
merge (`2881cfc`) and therefore predates the audio-tier and
pacer-timing fixes entirely, the exact same gap the original
486DX2-66+ViRGE datum had before it got re-confirmed. Only Mach64's
486DX2-50 cell went through the actual Round 1-3 fix-validation arc.
Not re-measured as part of this session's work since the operator
didn't direct it here -- flagging so "the matrix is done" isn't
overstated. See the Mach64 CPU-tier expansion section below.

**AUDIO-TIER MISMATCH BUG, found 2026-09-03, affects every recorded result
in this campaign.** The committed `vendor/passage/gameSource/music/SONG.WAV`
is the low-tier render (11025Hz mono), but every build/run this campaign
used `AUDIO_TIER=high` (22050Hz stereo) -- `musicPlayer.cpp` never checks
the loaded WAV's actual spec against the compile-time tier, so the mismatch
plays at the wrong rate ("wrong-speed, wrong-pitch playback," per that
file's own code comment). Confirmed on the physical target for every leg
(`DIR` = 2,998,844 bytes, the wrong file). `AUDIO_PRESENT` verdicts
throughout this doc and every `docs/benchmarks/` file confirm audio was
playing, never that it played correctly -- read them that way. `build_sha12`
does not cover this bug at all -- see each benchmark file's own Notes for
the full mechanism.

**Fix real-hardware-confirmed 2026-09-04, MERGED to `main` the same day
(`2881cfc`).** Round 1
(`docs/benchmarks/mach64-215ct-486dx2-50-round1-2026-09-04.md`) staged and
ran both a high-tier and a correctly-matched low-tier build on the same
486DX2-50 + Mach64 hardware: low-tier landed at ~14.79fps (same band as
every 486DX2-66 result), high-tier at ~9.9-10.2fps reported -- a ~4.75fps
delta, confirming the fps impact this doc's original text (below) predicted
would be *absent* was in fact real and large. **That original prediction
("fps numbers are not expected to be affected") was wrong, corrected here
rather than left standing**: the audio tier changes how much per-chunk
format-conversion work `SDL_DOSAudioPump` does (stereo/16-bit source vs.
mono/16-bit, both converted down to the device's fixed 8-bit-mono-22050Hz
format), and on this CPU tier that conversion cost is large enough to be
the dominant per-frame cost (~43% of every frame at the high tier, per
Round 1's per-stage diagnostic). **Round 1 also raised a tick-loss theory
for why the high-tier number might be further inflated -- Round 2's
in-game CMOS RTC witness directly measured this and found no loss
(`RTC elapsed = 304s = engine time(NULL)`, 0.0% gap); struck, see the
pacer-timing entry below.** Both fixes ship together in the same
`15fps`-owned patches (`0036`, `0038`-`0041` -- `0037` was a temporary
diagnostic, removed by `0038` -- plus the `make stage` guard), merged to
`main` 2026-09-04 as `2881cfc`.

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

**The CPU changed, 2026-09-03 -- partially re-checked by Round 1
(2026-09-04), not a full formal re-run of this gate.** 486DX2-50 +
Mach64's first datum came back at 13.685015fps, a real drop from the
~14.77-14.82fps band every 486DX2-66 run landed in. Round 1's per-stage
diagnostic build gives the actual answer for the audio-tier-corrected
low-tier case: **still pacer-bound, not compute-bound** -- low-tier
audio lands right at the 66.67ms budget (~67.6ms/frame measured, ~54ms
game work + ~12ms audio) and `fps_p50=15.00` sits exactly on the design
ceiling. The high tier is a different story: audio-conversion cost alone
(~43ms/frame) plus game work (~54ms) pushes every frame over budget --
genuinely compute-bound at the high audio tier on this CPU, which is
Phase 2's whole reason for existing (device-native audio format, L1c).
So: 486DX2-50 is pacer-bound at the low tier (matches 486DX2-66's
regime) and compute-bound at the high tier -- not a single regime for
this CPU, it depends on the audio tier. See
`docs/benchmarks/mach64-215ct-486dx2-50-2026-09-03.md` (original,
now-corrected datum) and
`docs/benchmarks/mach64-215ct-486dx2-50-round1-2026-09-04.md` (the
per-stage breakdown this conclusion is drawn from).

**GATE CLOSED, 2026-09-04, Round 3: the high tier is pacer-bound again
after the fix.** `patches/passage/0039`/`0040`/`0041` (device-native
audio, 16bpp blowup, silence-detect hint) collapse the high-tier audio
path's cost back down. Measured per-frame breakdown across all three
rounds (steady-state windows only, `STAGEDBG.LOG`, all high-tier audio
on 486DX2-50 + Mach64):

| Stage | Round 1 (D, pre-fix) | Round 2 (D2, L1c+L2 only) | Round 3 (D3, +silence-detect) |
|---|---|---|---|
| `render` | 32.3ms | ~31.3ms | (not separately re-quoted; steady) |
| `blowup` | 11.9ms (fixed cost) | **3.63-3.64ms** | ~3.64ms |
| `present` | 2.4ms | ~2.3ms | (steady) |
| `tail` | 7.4ms | ~7.3ms | (steady) |
| `pump` | 32.4ms (flat-out) | 0.00-0.74ms steady, **8.8-9.78ms in 3/15 windows** (song loop points) | **0.00-0.37ms every window, 0-22 calls** |
| `pacer` (yield/sleep) | 10.3ms (cooperative yield, frame over budget) | 12.71-21.27ms | 18.5ms slack average, real sleep |
| over-budget frames | ~300/300 every window | 3-4/300 steady windows, 67-77/300 in the 3 loop-point windows | 85/3900 total |
| reported avg fps | 9.90-10.19 | 14.72-14.82 (fails 14.90 KPI line by 0.08-0.18) | **14.97-15.02 (PASS)** |

Round 1's high tier: audio conversion alone ate ~43% of every frame,
genuinely compute-bound. Round 2's L1c+L2 fixes removed the *steady-state*
cost (`blowup` -11.85ms->3.64ms is the single biggest per-frame win) but
left a periodic cost concentrated in 3 windows per life, aligned to the
136s-long song's loop points -- the shared platform's silence-detect
throttle (`SDL_HINT_DOS_SILENCE_DETECT`) sleeping 10ms per pump call
during quiet passages where nothing gets written to the ring, still
enough to fail the KPI by a small margin (14.818fps average). Round 3's
`0041` (a one-line `SDL_HINT_DOS_SILENCE_DETECT=0` before `SDL_Init`)
removed that too -- `pump` is now near-zero in every window, not just
most of them, and the DX2-50 is back to a comfortable pacer-bound regime
(~18.5ms/frame slack) matching the 486DX2-66's original characterization,
now on the port's own minimum-target hardware. **KPI met: G's two Round
3 lives averaged 14.992fps.** See
`docs/benchmarks/mach64-215ct-486dx2-50-round2-2026-09-04.md` and
`docs/benchmarks/mach64-215ct-486dx2-50-round3-2026-09-04.md`.

**Methodology lesson from this gate's failure mode, worth generalizing
past this one campaign (hub-worthy -- recorded here first, per this
project's own policy of landing genuinely general findings in the hub
only as a deliberate, separate step, not as a side effect of a port-local
doc pass).** A per-frame *game-loop* work measurement -- summing render,
blowup, present, tail, i.e. the spans the main loop itself executes --
looks like it answers "are we compute-bound," but it silently excludes
whatever a cooperative background thread does *during the pacer's own
sleep*. On this port, the audio thread's `SDL_DOSAudioPump` iterates
cooperatively inside that sleep window, not inside any span the game
loop's own instrumentation sees. The result: a CPU can show a healthy
"slack, not compute-bound" reading right up until that background cost
grows large enough to consume the sleep entirely and start spilling into
frame time -- at which point it appears in full, all at once, rather
than growing visibly in the measurement first. That is exactly how 43%
of the frame stayed invisible to a loop-only measurement at the 486DX2-50
tier (Round 1) and how a much smaller version of the same blind spot
(the silence-detect throttle) still cost 0.08fps against the KPI even
after the main fix landed (Round 2). **Practical consequence: the
Section 1 compute-bound gate must be re-run per CPU with the
background/cooperative work counted, not just the game loop's own
spans** -- either via a diagnostic build that instruments the pacer's
sleep/yield span directly (this campaign's `STAGEDBG.LOG` approach), or
via an A/B run pair across the two audio tiers on the CPU in question, if
a dedicated diagnostic build isn't available. A clean "slack" reading
from loop spans alone is necessary, not sufficient.

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

**Historical note, superseded -- kept for the record, not current advice.**
The paragraph that stood here through the first DOSBox-X smoke described a
60.4%-rejected, clustering-on-16.67 result and recommended a standalone
probe before trusting `fps_p50`/`fps_p95` at all. That was about patch
`0034`'s *first* version, before its own reject-filter fix. The full,
current story -- `0034`'s reject-filter fix, `0035`'s definitional fix, and
the confirmed-real (not DOSBox-X-only) bimodal pacing finding that resulted
-- is in the Known-open-items section below; read that instead of treating
this paragraph as live guidance.

## The KPI, written down in advance

Per card, on the 486DX2-66 + PicoGUS rig, build `f1f867ccadad`:

- **PASS:** sustained fps within **0.3 fps** of the card's established figure
  (Cirrus 14.94, ViRGE 14.16-14.5), audio present throughout, no visual
  corruption against `docs/screenshots/dossage-gameplay.png`.
- **FAIL:** outside that band, or audio absent, or corruption.
- **INVALID, not a datum:** hash mismatch on the staged binary; wall-clock
  bracket disagreeing with the engine's own `Game time` by more than the
  established ~14-16 s startup overhead; UniVBE not confirmed active.
- **Mach64 has no established figure** -- see its gate below. Its first run
  is exploratory and cannot pass or fail against a band that does not exist.

Every result **from `f1f867ccadad` onward** is attributed to
`build_sha12 = f1f867ccadad` (re-baselined 2026-09-02, third time same day
-- see "The build pin" below). The two Cirrus runs recorded under
`9c90db0e7905` (`docs/benchmarks/cirrus-cl-gd5434-2026-09-02.md`) predate
this and are correctly attributed to that earlier pin in their own files --
this section documents the *current* pin for new results, not a
retroactive relabeling of old ones.

### The build pin

**`build_sha12 = f1f867ccadad`**, re-baselined 2026-09-02 (supersedes
`9c90db0e7905`, which supersedes `cf5f9a5a861e` from earlier the same day
-- both used to record real results, unlike the untraceable `9cace45a`
before them; see "Why this was re-baselined" below). Recomputable at any
time:

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
`sha256 18ab16ff303ae076e6ac46e54ab117683eb56ce666ca825785b5545ddb3b3803`
at `AUDIO_TIER=high` (the default). Independently reproduced (this repo's
own build plus `build-qa-0034`'s separate rebuild), byte-identical both
times. `9c90db0e7905`'s own binary
(`sha256 8480da383842a4b273dce263b144fcf0980d839ebcd36126c651126de1775afd`)
remains valid history, just not the current pin -- patch `0035` changed
`game.cpp` again after it, which is why the hash moved.

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

Originally written as one CPU (486DX2-66), one sound card (PicoGUS, SB
mode), three video cards ("not a full cross-product -- there is no reason
to re-sweep CPU or audio"). **That CPU-scoping decision was reversed
2026-09-03, operator-directed**: the audio-fixed, card-varying part of that
reasoning still holds (no reason to re-sweep PicoGUS/SB mode), but CPU is
now an active second dimension. Reason it's worth doing, now that there's
real data: 486DX2-66 numbers all landed near the pacer ceiling
(~14.77-14.82fps across all three cards) precisely because that CPU has
slack to spare -- a genuinely different, slower CPU is exactly the case
Section 1's compute-bound gate exists to catch a regime change in, and the
first 486DX2-50 datum already shows one (see Section 1, above).

### 486DX2-66 video-card matrix (complete)

| Card | State | What this run is for |
|---|---|---|
| Cirrus CL-GD5434 | **re-confirmed 2026-09-02, twice: 14.82 fps (`9c90db0e7905`), 14.77 fps (`f1f867ccadad`)**; **re-confirmed again 2026-09-05, 14.966555 fps, on the merged fix build** (`build_sha12=a5e9835f12e7`) | Reference/repeatability. Runs banked -- SDL/0019 force-disables LFB for a genuine aperture defect. The 2026-09-05 re-confirmation also found a small, reproducible "one second over" fps effect specific to this CPU tier on the banked path -- see `docs/benchmarks/cirrus-cl-gd5434-486dx2-66-2026-09-05.md` for the full finding; earlier runs are `docs/benchmarks/cirrus-cl-gd5434-2026-09-02.md` and `...-f1f867ccadad.md`. |
| S3 ViRGE 86C375 | **re-confirmed 2026-09-03, 14.82 fps** (build `f1f867ccadad`); **re-confirmed again 2026-09-04, 15.016779 fps, on the merged fix build** (`build_sha12=a5e9835f12e7`) -- closes the "never independently re-measured" audio-tier-fix gap that run's own Notes flagged. | Reference/repeatability. Uses LFB at 320x240x16 -- see `docs/benchmarks/virge-86c375-2026-09-03.md` and `docs/benchmarks/virge-86c375-486dx2-66-2026-09-04.md`. |
| ATI Mach64 215CT/-ET | Gate passed, first datum 2026-09-03: 14.77 fps (build `f1f867ccadad`, pre-merge). **Re-confirmed 2026-09-05 on the merged build (`build_sha12=a5e9835f12e7`), PASS: 15.016779 fps**, clean 298s signature -- closes the campaign's last pairing still exclusively on the pre-merge build. | Gated and measured -- see below, `docs/benchmarks/mach64-215ct-2026-09-03.md`, and `docs/benchmarks/mach64-215ct-486dx2-66-2026-09-05.md`. |

### Mach64 CPU-tier expansion (CLOSED, 2026-09-04; partially stale as of 2026-09-05, see correction below)

Roster from `profiles/dossage.yaml`'s `machines` table / the hub's
`HARDWARE.md` primary matrix: 486DX2-66, 486DX2-50 (dossage's own tracked
`dos_minimum_target`/`dos_recommended_target` per the hub's `ports.yaml`),
Am5x86-133, Pentium OverDrive 83. Started with Mach64 first since it was
already staged and its `SDL_HINT_DOS_FORCE_MODE_ID` requirement is
confirmed CPU-independent. **All four CPUs have a Mach64 datum; the
486DX2-50 leg's fix-validation arc closed at KPI PASS.**

**Correction, 2026-09-05, found while closing out the ViRGE/Cirrus
sweeps**: this section's "nothing left queued for Mach64" and the table
below's "on the current, merged build" phrasing overstated it. Only
486DX2-50 actually went through the fix-validation arc and reflects the
audio-tier/pacer-timing fixes. The Am5x86-133 and POD83 rows below are
both still `build_sha12=f1f867ccadad`, which predates the `2881cfc`
merge -- the exact same "never independently re-measured against the
fix" gap the original 486DX2-66+ViRGE datum had before it got
re-confirmed (`docs/benchmarks/virge-86c375-486dx2-66-2026-09-04.md`).
Not re-measured as part of the ViRGE/Cirrus sweep work since the
operator didn't direct it there -- flagging rather than silently
carrying the overstatement forward.

**Am5x86-133 re-measured on the merged build, 2026-09-05 -- and it
revised the Cirrus sweep's own conclusion, not just closed a gap.**
`docs/benchmarks/mach64-215ct-am5x86-2026-09-05.md`: PASS, but landed
on the "one second over" 299s/14.966555fps shape -- the same effect
Cirrus's 486DX2 tiers showed, at a CPU tier that hit the clean
298s/15.016779fps signature on BOTH ViRGE and Cirrus. That breaks the
"clean CPU-tier boundary, not a gradient" reading the Cirrus sweep
reached (see that section below) -- see this file's Notes for the
revised picture (framebuffer-byte-count and/or boundary-adjacent
run-to-run jitter, neither confirmed via instrumentation) and a
re-reading of Round 3's own G1/298s-G2/299s-D3/299s split on identical
486DX2-50+Mach64 hardware as supporting evidence for jitter over strict
determinism.

**POD83 re-measured on the merged build, 2026-09-05, same day: PASS at
15.016779fps, the clean 298s signature** -- unlike Am5x86-133 (a
slower CPU) at the same card/build, which landed at 299s. Same
`fps_p95` (14.96) as that leg despite the different rounding outcome,
consistent with the jitter reading above rather than the effect being
fixed by CPU speed. See `docs/benchmarks/mach64-215ct-pod83-2026-09-05.md`.

**CLOSED, 2026-09-05, same day: Mach64+486DX2-66 re-confirmed on the
merged build, PASS at 15.016779fps, clean 298s signature.** This was
the last pairing in the whole 3-card x 4-CPU matrix still exclusively
on the pre-merge build -- **every cell (Mach64/ViRGE/Cirrus x
486DX2-50/486DX2-66/Am5x86-133/POD83, twelve pairings) is now measured
on `build_sha12=a5e9835f12e7` and PASSES.** See
`docs/benchmarks/mach64-215ct-486dx2-66-2026-09-05.md`, which also
carries the full twelve-cell "one second over" summary table -- the
effect does not reduce to CPU tier, video path, or framebuffer size
alone; ViRGE never shows it, the other two cards each show it on a
different, non-overlapping subset of CPU tiers, and 486DX2-50+Mach64
(the tightest-margin pairing) has flipped between both outcomes across
repeat lives. Working explanation: overall margin sets how close a
pairing sits to the rounding boundary, and per-life timing jitter
decides which side any individual life lands on for anything close
enough. Does not threaten the KPI anywhere. Nothing left queued in this
matrix.

Each CPU swap needs its own video-card sweep in principle -- Mach64's is
done, Cirrus's and ViRGE's are not. **Next up, per the operator: ViRGE.**
Only 486DX2-66 has a ViRGE datum so far (see the 486DX2-66 matrix,
above); 486DX2-50/Am5x86-133/Pentium OverDrive 83 do not yet, and none of
the three have been re-run against the merged `main` build at all. Not
started as of this writing.

| CPU | Card | State |
|---|---|---|
| 486DX2-50 | ATI Mach64 215CT/-ET | **CLOSED, 2026-09-04, PASS.** First datum 2026-09-03: 13.685015fps (build `f1f867ccadad`) -- not a clean reference figure, that build carried the mis-staged low-tier `SONG.WAV` under an `AUDIO_TIER=high` compile (see banner above). Round 1 (`...-round1-2026-09-04.md`) validated the pacer-timing (0036) and audio-tier fixes on real hardware: low-tier ~14.79fps (clean), high-tier ~9.9-10.5fps (both stand as measured -- Round 1's own tick-loss theory was floated then refuted by Round 2's RTC witness, no clock-loss correction needed). Round 2 (`...-round2-2026-09-04.md`) validated Phase 2 (device-native audio/0039, 16bpp blowup/0040) but the high tier still failed the KPI by 0.08fps (14.818 vs. 14.90), root-caused to an audio silence-detect throttle cost. Round 3 (`...-round3-2026-09-04.md`) validated the fix (`0041`): **PASS, mean 14.992fps** across two lives, matching the design ceiling and the 486DX2-66's original regime. See `docs/benchmarks/mach64-215ct-486dx2-50-2026-09-03.md` (original datum, corrected in place) and all three round files for the full arc. |
| Am5x86-133 | ATI Mach64 215CT/-ET | First datum 2026-09-03: 15.016779 fps (build `f1f867ccadad`, pre-merge). **Re-confirmed 2026-09-05 on the merged build (`build_sha12=a5e9835f12e7`), PASS: 14.966555 fps** -- but landed on the "one second over" (299s) shape, unlike this CPU's own clean ViRGE/Cirrus results. Hit and recovered from a real stale-UVCONFIG video hazard (physical OUT OF RANGE) mid-session first. See `docs/benchmarks/mach64-215ct-am5x86-2026-09-03.md` (original) and `docs/benchmarks/mach64-215ct-am5x86-2026-09-05.md` (merged-build re-confirmation and the revised "one second over" finding). |
| Pentium OverDrive 83 | ATI Mach64 215CT/-ET | First datum 2026-09-03: 14.966555 fps (build `f1f867ccadad`, pre-merge). **CPU identity RESOLVED**: two real dinspect bugs (an unbounded TSC-calibration hang leaving a stale Am5x86-era report behind, then RDTSC itself faulting under EMM386 on this part once the hang was fixed) -- confirmed three independent ways (fixed dinspect, an independent CPUID dump, and PhoenixBIOS's own POST text via the rig's hardware camera) that this is genuinely Intel Pentium OverDrive, ~83MHz. **Re-confirmed 2026-09-05 on the merged build (`build_sha12=a5e9835f12e7`), PASS: 15.016779 fps** -- the clean 298s signature this time, unlike the Am5x86-133+Mach64 leg the day before (299s) despite POD83 being the faster CPU; same `fps_p95` (14.96) as that leg, consistent with boundary-adjacent jitter rather than a fixed per-tier outcome. Closes the last CPU/card pairing that was exclusively pre-merge -- except Mach64+486DX2-66, see the correction below. See `docs/benchmarks/mach64-215ct-pod83-2026-09-03.md` (original) and `docs/benchmarks/mach64-215ct-pod83-2026-09-05.md` (merged-build re-confirmation). |

### ViRGE CPU-tier expansion (CLOSED, 2026-09-04)

Same roster and same outer-loop-card/inner-loop-CPU structure as the
Mach64 sweep above (`docs/rig-runbook.md` section 2a). All four cells
run the same day, directly on the merged fix build (`2881cfc`) -- no
fix-validation arc needed here, that already happened on Mach64. **All
four CPU tiers now have a ViRGE datum; every one PASSED, and every one
that had enough pacer slack (all but 486DX2-50, which still PASSED
comfortably) landed on the exact same 15.016779fps/4475-frame
signature** -- five independent CPU/card combinations now confirm this
is the design's deterministic ceiling-hit figure for an uninterrupted
natural life, not coincidence. Nothing left queued for ViRGE.

| CPU | Card | State |
|---|---|---|
| 486DX2-66 | S3 ViRGE 86C375 | **Re-confirmed 2026-09-04, PASS: 15.016779 fps** on the merged-fix build (`build_sha12=a5e9835f12e7`) -- see the 486DX2-66 matrix, above, and `docs/benchmarks/virge-86c375-486dx2-66-2026-09-04.md`. Listed here too since it's now been run on the same current build as the rest of this table, not just the original 486DX2-66 sweep. |
| 486DX2-50 | S3 ViRGE 86C375 | **DONE, 2026-09-04, PASS: 15.016779 fps** (`build_sha12=a5e9835f12e7`, the merged-fix build -- new pin, supersedes `f1f867ccadad`). Bit-for-bit match to Round 3's Mach64+486DX2-50 G2 life (same frame count, same fps to six decimals, same `fps_p50`/`fps_p95`) -- confirms the audio-tier fix's benefit transfers cleanly to ViRGE's LFB path, closing the exact gap the 486DX2-66 ViRGE leg's own Notes flagged as untested. See `docs/benchmarks/virge-86c375-486dx2-50-2026-09-04.md`. |
| Am5x86-133 | S3 ViRGE 86C375 | **DONE, 2026-09-04, PASS: 15.016779 fps** (`build_sha12=a5e9835f12e7`). Fourth appearance of the exact same fps/frame-count signature seen on both 486DX2-tier ViRGE legs today and the original Am5x86-133+Mach64 datum -- reads as the pacer's deterministic ceiling-hit signature, not coincidence. CPU identity (AMD Am5x86, ~133MHz, FPU YES) confirmed clean on the fixed `dinspect` build, no repeat of the original ~100MHz/FPU:no misread. See `docs/benchmarks/virge-86c375-am5x86-2026-09-04.md`. |
| Pentium OverDrive 83 | S3 ViRGE 86C375 | **DONE, 2026-09-04, PASS: 15.016779 fps** (`build_sha12=a5e9835f12e7`). Fifth appearance of the exact same fps/frame-count signature seen on every other pacer-slack-having CPU this sweep -- now fully established, not just suggestive. `fps_p95=14.97`, completing a monotonic progression with CPU speed across the whole sweep. RDTSC/EMM386 hazard from the original POD83 leg re-checked and did not recur. **Closes the ViRGE CPU-tier sweep: all four CPU tiers now have a ViRGE datum.** See `docs/benchmarks/virge-86c375-pod83-2026-09-04.md`. |

### Cirrus CPU-tier expansion (CLOSED, 2026-09-05)

Same structure as the Mach64/ViRGE sweeps. Cirrus previously had only a
486DX2-66 datum, and it predates the merge
(`docs/benchmarks/cirrus-cl-gd5434-2026-09-02.md`,
`...-f1f867ccadad.md`). First cell run directly on the merged fix build
-- a full card swap (ViRGE -> Cirrus) combined with a CPU swap
(Am5x86-133 -> POD83) in one operator action, so this also hit (and
recovered from) the same UVCONFIG-not-yet-run gap the original ViRGE
swap did. **All four CPU tiers now have a Cirrus datum; every one
PASSED.** Unlike the ViRGE sweep, the fps/frame-count signature was
NOT uniform: the two 486DX2 tiers (50, 66) both land one second over
the ideal life duration (299s/14.966555fps, reproduced across three
lives total), while the two faster tiers (Am5x86-133, POD83) both land
on the usual 298s/15.016779fps figure. Does not threaten the KPI at
any tier. Nothing left queued for Cirrus.

**Revised, 2026-09-05, same day: this is not actually a "CPU-tier
boundary in Cirrus's banked-path cost."** The paragraph above (and the
Am5x86-133 row's original text below) read the split as clean and
tier-determined. The very next cell run afterward --
Mach64+Am5x86-133 on the merged build,
`docs/benchmarks/mach64-215ct-am5x86-2026-09-05.md` -- showed the same
"one second over" effect on Mach64 at this exact CPU tier, which had
been clean on both Cirrus and ViRGE. A boundary that isn't fixed to
CPU tier isn't a CPU-tier boundary. See that file's Notes for the
revised picture (framebuffer byte count and/or per-life timing jitter
near the rounding threshold, neither confirmed via instrumentation) and
the Mach64 CPU-tier expansion section for the correction in context.

| CPU | Card | State |
|---|---|---|
| 486DX2-50 | Cirrus CL-GD5434 | **DONE, 2026-09-05, PASS: 14.966555 fps** (`build_sha12=a5e9835f12e7`), clearing the 14.90fps KPI line with real margin. Same 299s/4475-frame shape as the 486DX2-66+Cirrus leg, exactly -- the banked-path effect did NOT get worse at this tighter-margin CPU as feared going in; it's identical in magnitude at both tiers, which argues against a pure CPU-cycle-cost mechanism (see that file's Notes). `fps_p95=14.79`, matching ViRGE+486DX2-50's `14.80`. Zero rejected samples, zero critical/warn counts -- none of the original pre-fix pacer/audio-tier signatures reappeared. Completes KPI confirmation on all three cards at the port's minimum-target CPU. See `docs/benchmarks/cirrus-cl-gd5434-486dx2-50-2026-09-05.md`. |
| 486DX2-66 | Cirrus CL-GD5434 | **Re-confirmed 2026-09-05, PASS: 14.966555 fps** (`build_sha12=a5e9835f12e7`, reproduced identically across two lives). **Breaks the campaign's 15.016779fps/4475-frame signature** -- lands at 299s instead of 298s (same frame count), the first pacer-slack-having CPU/card combination to do so. Working explanation: Cirrus's banked-path overhead, easily absorbed by faster CPUs (POD83), apparently tips 486DX2-66's tighter margin over the integer-second rounding boundary. Small (does not threaten the KPI), real (reproduced twice), not yet root-caused via dedicated instrumentation. See `docs/benchmarks/cirrus-cl-gd5434-486dx2-66-2026-09-05.md`. |
| Am5x86-133 | Cirrus CL-GD5434 | **DONE, 2026-09-05, PASS: 15.016779 fps** (`build_sha12=a5e9835f12e7`). Back to the usual 298s/4475-frame ceiling-hit signature, NOT the "one second over" pattern the two 486DX2 tiers showed. `fps_p95=14.96`, no measurable video-path penalty at this tier *on Cirrus specifically*. **Correction, same day**: this was read at the time as confirming a clean CPU-tier boundary -- wrong, this same CPU tier shows the effect on Mach64 (see above and `docs/benchmarks/mach64-215ct-am5x86-2026-09-05.md`), so it isn't tier-determined after all. **Closes the Cirrus CPU-tier sweep: all four CPU tiers now have a Cirrus datum.** See `docs/benchmarks/cirrus-cl-gd5434-am5x86-2026-09-05.md`. |
| Pentium OverDrive 83 | Cirrus CL-GD5434 | **DONE, 2026-09-04, PASS: 15.016779 fps** (`build_sha12=a5e9835f12e7`). Sixth appearance of the campaign's fps/frame-count signature, and the first on a banked (not LFB) card. **Correction, 2026-09-05**: this was read at the time as proving the signature video-path-independent -- wrong, see the 486DX2-66 row above and `docs/benchmarks/cirrus-cl-gd5434-pod83-2026-09-04.md`'s own correction note. `fps_p95=14.96`, consistent with the ViRGE+POD83 leg's `14.97` (card doesn't move `fps_p95`, only CPU tier does -- that part of the finding still holds). See `docs/benchmarks/cirrus-cl-gd5434-pod83-2026-09-04.md`. |

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

**Ran 2026-09-03, and it landed somewhere unexpected: FAILED on the first
attempt, then fixed.** The negotiated mode was **512x384 16bpp**, not
640x480 -- a mode this specific rig's VGA capture stick cannot lock onto at
all (operator decision, `docs/VIDEO-SWAP.md` in the `vcctrl` repo,
2026-08-19: "stop running the mach64 in 512x384 ... it must be 640x480").
Root cause: core SDL3's `SDL_GetClosestFullscreenDisplayMode()` (vendored,
not a DOS-layer or dossage patch) ties on aspect ratio -- 320x240, 512x384,
and 640x480 are all exactly 4:3 -- and always keeps the smallest qualifying
candidate on a tie. Not caused by `PIN_WINDOW_TO_NATIVE_MODE` (that hint
cannot fire on the very first mode-set). This exact case is the documented
motivating example for the shared layer's `SDL_HINT_DOS_FORCE_MODE_ID`
hint, and doskutsu hit the identical bug on the identical
Mach64/UniVBE-6.70 combination (their own `docs/internal/BOOT.md`,
2026-08-19 -- the same investigation this rig's operator note traces to).

**Fix, and it needs to travel with every future Mach64 launch on this
rig**: `SET SDL_HINT_DOS_FORCE_MODE_ID=0x0111` before running
`DOSSAGE.EXE`. Confirmed working (`DOSVESA-MODESET: id=0x0111 640x480
16bpp`, capture stick went `frozen` -> `locked`, visual diff clean). This
is a launch-environment requirement, not a one-time workaround -- dossage
has no boot-menu/launcher infra yet to bake it in (see
`profiles/dossage.yaml`'s TBD notes), so it must be set by hand or scripted
every time until that exists. Full story:
`docs/benchmarks/mach64-215ct-2026-09-03.md`.

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
- **`fps_p50`/`fps_p95` measure the wrong window, AND the underlying
  clock artifact -- BOTH ROOT-CAUSED, 2026-09-03. See the closing
  paragraph at the end of this entry for the final answer; the rest is
  kept as the historical chase that got there.** Both fields still cluster above the
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
  don't compare it across cards as a simple number yet.
  **Cross-card confirmation, 2026-09-03: three cards, three chip
  families, one identical result -- the video card is ruled out as the
  cause entirely.** `fps_p50=16.67`/`fps_p95=9.09`, near-zero reject
  rate (0.22-0.27% across all three), now hold identically on Cirrus
  (banked), S3 ViRGE (LFB), and ATI Mach64 (LFB, forced 640x480 via
  `SDL_HINT_DOS_FORCE_MODE_ID` -- see the Mach64 gate section above).
  Same two clusters regardless of banked vs. LFB rules out the
  framebuffer-write path; same two clusters across three unrelated chip
  vendors rules out anything card-specific at all. What's left constant
  across all three runs is the CPU (486DX2-66) and the build -- pointing
  squarely at a CPU/system-timer-level mechanism.

  **Strengthened further, same day: also confirmed CPU-clock-speed-
  independent.** The 486DX2-50+Mach64 run (see CPU-tier expansion, above)
  produced the exact same `fps_p50=16.67`/`fps_p95=9.09` -- bit-for-bit
  identical to every 66MHz run -- despite the plain average moving a full
  1.1fps (14.77 -> 13.69) between the two CPU tiers. A ~24% CPU clock
  difference changing the real average but leaving these two specific
  values completely untouched is hard to explain with any CPU-cycle-count
  -based mechanism (expected to scale with clock speed) and is strong,
  independent evidence for a fixed-frequency **hardware timer** cause
  (e.g. the PC BIOS/PIT tick, driven by its own oscillator, not the CPU
  clock) over a workload- or CPU-speed-dependent one.

  **Fifth-axis confirmation, same day: a third, non-Intel CPU vendor.**
  The Am5x86-133+Mach64 run produced the identical
  `fps_p50=16.67`/`fps_p95=9.09` yet again -- this time on an AMD part
  (vs. the two Intel 486DX2 tiers), and the fastest plain average of the
  entire campaign (15.02fps, essentially at the design ceiling) sitting
  right next to the exact same two unchanged percentile values. 5
  independent axes now agree exactly: 3 video chip vendors x 3 CPUs
  (486DX2-66, 486DX2-50, Am5x86-133) spanning both Intel and AMD and a
  13.69-15.02fps range in the actual average. Still doesn't pin down
  either cluster's exact source (110ms plausibly 2x the PC BIOS/PIT
  tick, ~54.925ms; 60ms unidentified) -- **queued as a standalone
  `Time::getCurrentTime()`/pacer-timing probe** (isolated from the full
  game) -- two independent sessions (build-qa, vcctrl-c3) both converged
  on recommending this rather than guessing further from full-game data,
  and the evidence now makes it more likely to actually land somewhere
  specific rather than come back inconclusive. Written up as a
  standalone, self-contained investigation brief:
  `docs/PACER-TIMING-INVESTIGATION.md`. Full data:
  `docs/benchmarks/cirrus-cl-gd5434-2026-09-02-f1f867ccadad.md`,
  `docs/benchmarks/virge-86c375-2026-09-03.md`,
  `docs/benchmarks/mach64-215ct-2026-09-03.md`,
  `docs/benchmarks/mach64-215ct-486dx2-50-2026-09-03.md`,
  `docs/benchmarks/mach64-215ct-am5x86-2026-09-03.md`.

  **ROOT CAUSE CONFIRMED, 2026-09-03, `docs/PACER-TIMING-INVESTIGATION.md`
  ("RESOLVED" section).** Not a hardware timer in the vague sense
  guessed above, but specifically: DJGPP's `gettimeofday()` (what
  `Time::getCurrentTime()` calls) derives its entire sub-second
  resolution from DOS's own hundredths-of-a-second clock (`INT 21h
  AH=2Ch`), which is itself driven by the 18.2065Hz BIOS/PIT tick --
  confirmed two independent ways: DJGPP's official libc reference
  documentation (states exactly this), and disassembly of this port's
  actual compiled `libc.a` (`gettimeo.o`) confirming the binary really
  executes that path. Since 100Hz doesn't divide evenly into 18.2065Hz,
  the tick-to-hundredths conversion advances unevenly, and the pacer's
  genuinely-consistent ~66.6667ms delivered period (converged against a
  *different* clock, `uclock()`/`SDL_GetTicksNS()`) always spans either 1
  or 2 ticks -- collapsing every `gettimeofday()`-measured sample onto
  ~55ms or ~110ms depending on tick phase, regardless of card, CPU speed,
  or CPU vendor, because the true period being measured is itself
  hardware-independent by the pacer's own design. **Exactly confirmed**
  against real `PACESIM.LOG` raw per-sample data (not just a bucketed
  histogram): all 256 checked samples are exactly one of {50, 60, 100,
  110, 330}ms, zero exceptions -- DOS's hundredths counter advances by 5
  or 6 per BIOS tick in a near-deterministic alternating pattern, so
  2-tick readings land almost entirely on exactly 110ms rather than a
  spread. (An earlier pass here cited 55.13ms/109.59ms "island centers"
  computed from coarse histogram-bucket midpoints -- superseded by this
  exact check, corrected per this project's own discipline rather than
  left stale.) Reproduces in complete isolation (`tests/probes/pacesim.c`, no SDL, no
  rendering, no audio, no engine) -- rules out anything full-game-
  specific. **Fix implemented, DOSBox-X-confirmed, real-hardware-confirmed,
  and merged to `main` 2026-09-04 as `2881cfc`**: `patches/passage/0036`,
  originally committed `60ad807`
  on branch `dx2-50-15fps` (isolated worktree
  `/home/claude/git/dossage-dx2-50`), re-bases the capture onto
  `SDL_GetTicksNS()` -- the pacer's own clock -- instead of
  `gettimeofday()`. DOSBox-X smoke (correctness only) across three builds
  shows `fps_p50` collapsed from `16.67` to exactly `15.00` (the design
  ceiling), `fps_p95` now `14.77-14.83`, reject rates 0-1/4475.

  **Round 1, real hardware, 2026-09-04: CONFIRMED.** 486DX2-50 + Mach64,
  five lives across three staged builds (high-tier audio, low-tier
  audio, and a per-stage-cost diagnostic build, all carrying patch 0036).
  Percentiles now vary meaningfully per life instead of the fixed
  `16.67`/`9.09` artifact seen on every pre-fix run this campaign; reject
  rates 0 across every life; low-tier `fps_p50=15.00` lands exactly on
  the design ceiling as predicted. Full result, and the audio-tier fix's
  own real-hardware confirmation (see banner above): `docs/benchmarks/
  mach64-215ct-486dx2-50-round1-2026-09-04.md`.

  **Round 1 raised a tick-loss theory; Round 2 REFUTED it, 2026-09-04
  -- corrected here, not left standing.** Round 1 saw 80-166s unexplained
  launch-to-title wall-clock gaps on high-tier-audio runs only, ruled out
  WAV disk-load time as the cause (a timed `COPY` of both WAV files), and
  hypothesized sustained high-tier-audio load was making the BIOS/PIT
  tick lose ticks -- which would inflate every in-game clock
  (`gettimeofday()`, `uclock()`, `SDL_GetTicksNS()`) together, since all
  derive from that same tick. **Round 2's dedicated diagnostic build
  (`build/stage-D2`) added an in-game CMOS RTC witness -- an independent
  32.768kHz-crystal clock, not derived from the BIOS/PIT tick -- and
  measured zero loss**: `"RTC elapsed = 304s (engine time(NULL) = 304s;
  engine clock gained 0.0%)"` for a full real life. **There is no clock
  loss on this rig.** The wall-clock gaps are now understood to be
  ordinary startup/title-wait variability instead (the title screen
  waits for a key or joystick event to start the first life, and a
  floating gameport -- "Found 1 joysticks" in every run -- can deliver a
  spurious button event at an unpredictable moment). Round 1's high-tier
  fps figures (~9.9-10.5fps reported) stand as measured, not as upper
  bounds. Full reasoning and the RTC witness's exact output:
  `docs/benchmarks/mach64-215ct-486dx2-50-round2-2026-09-04.md`.

  **Round 2 also located the true, small remaining source of Round 1's
  ~5.7s-per-life shortfall against the 298.3s ideal 15fps life**: three
  ~20s windows per life (starting at the song's loop points, song length
  136s) show the audio pipeline's silence-detect throttle (shared-layer
  `SDL_HINT_DOS_SILENCE_DETECT`, doskutsu's `SDL/0054`) sleeping 10ms per
  pump call during quiet passages where nothing is written to the ring
  -- a real, if small, per-frame cost during those windows only. Fix is
  a one-line game-side hint (`SDL_HINT_DOS_SILENCE_DETECT=0`) before
  `SDL_Init`, landed as `patches/passage/0041`. **Round 3 validated it,
  2026-09-04: KPI PASS, mean 14.992fps -- see the "GATE CLOSED" table
  earlier in this document and
  `docs/benchmarks/mach64-215ct-486dx2-50-round3-2026-09-04.md`.** Merged
  to `main` with the rest of the arc as `2881cfc`.
