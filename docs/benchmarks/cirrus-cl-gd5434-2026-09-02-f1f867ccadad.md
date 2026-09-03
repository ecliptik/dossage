# Benchmark

## Hardware
CPU: 486DX2, ~66MHz
Clock: 66MHz
L1: (not measured)
L2: (not measured)
RAM: 48 MB
Video: Cirrus Logic CL-GD5434 PCI, 1 MB VRAM (see
  `cirrus-cl-gd5434-2026-09-02.md` for the identity confirmation --
  unchanged, same card, same session)
Sound: PicoGUS v2, Sound Blaster mode (`picogus-sb-dbop13` firmware v4.1.1,
  port 220, IRQ 7, DMA 3, AdLib port 388)
MIDI: n/a (SB mode this run)
DOS: MS-DOS 6.22
VESA: UniVBE 6.70, VBE 3.0, banked

Hardware ID: HW-486-66

## Build
Commit: `8e0540a` in `vendor/passage`'s own tracked history (patch
  `patches/passage/0035-dos-paced-period-fps-percentile-fix.patch`,
  landed on top of `0034`'s `d43465d` -- 0034's own diff is untouched)
Upstream revision: Passage `2f713f261dc907f6feda106ebbee0464eeab791d`,
  minorGems `ef42b1ce511f2d355d5fc898fcce0b0af3a76d62`
SDL revision: `74a746281f2208e07a7680560fcb7ec57565228e`
shared/ (.sdl-dos-ports/) pin: unchanged from
  `cirrus-cl-gd5434-2026-09-02.md`
Optimization flags: unchanged from `cirrus-cl-gd5434-2026-09-02.md`

`build_sha12 = f1f867ccadad` (supersedes `9c90db0e7905` -- `game.cpp`
changed again to fix `fps_p50`/`fps_p95`'s measured quantity, see
`docs/BENCHMARK-PLAN.md`'s Known-open-items entry for the full story).
`sha256(exe) = 18ab16ff303ae076e6ac46e54ab117683eb56ce666ca825785b5545ddb3b3803`.

## Scene
Game: Passage
Level/room: n/a
Save file: n/a
Duration: 303s (life 1, natural completion)

## Results
Average FPS: **14.768977** (4475 frames / 303s, `time(NULL)`-based)
1% low: RUNMANIFEST `fps_p50=16.67`, `fps_p95=9.09` -- **now trustworthy
  as a measurement** (see Notes: near-zero reject rate on both DOSBox-X
  and real hardware confirms the capture is measuring the real
  distribution, not an artifact), but the KPI's original intent (p95 as
  "the slow 5% of frames," expected below 15fps) needs re-reading in
  light of what this run actually found -- see Notes.
Simulation rate: locked at 15fps target; still pacer-bound, unchanged
  from Section 1 gate.
Audio underruns: not instrumented. Two `vcctrl_audio_verdict` checks,
  both `AUDIO_PRESENT`.
Peak memory: not measured.
Notes:
- **PASS, second confirmation.** 14.768977fps is 0.171fps from the
  ~14.94fps reference -- inside the 0.3fps band, consistent with the
  first Cirrus run's 14.817881fps (`cirrus-cl-gd5434-2026-09-02.md`).
  Two real-hardware runs on two different builds (`9c90db0e7905` and
  `f1f867ccadad`) both land in-band; the ~0.05fps difference between
  them is ordinary run-to-run variance, not a regression signal.
- **The core finding of this run: `fps_p50`/`fps_p95` are now an
  accurate measurement of a genuinely bimodal per-frame pacing
  distribution, not a bug.** Patch `0035` fixed the previous
  definitional issue (measuring work-time instead of the full paced
  period); the reject-filter now barely engages on either environment
  (DOSBox-X: 0.02% rejected; real hardware, this run: 0.22% rejected,
  10/4475) -- both essentially measuring the complete real distribution.
  **Real hardware reproduces DOSBox-X's exact numbers**:
  `fps_p50=16.67` (~60ms) on both; `fps_p95=9.09` (~110ms) on both, in
  every full-life block seen across three runs so far. This rules out
  the "DOSBox-X-only timer artifact" hypothesis this same investigation
  raised earlier the same day -- it is a real, environment-independent
  characteristic of this port's pacer on this CPU tier, not a
  measurement or emulator problem.
- **Working (unconfirmed) explanation for the two clusters**: 110ms is
  close to 2x the classic PC BIOS/PIT tick (~54.925ms, 18.2Hz) --
  9.09fps computed from 109.85ms is a near-exact match. 60ms does not
  cleanly match any small integer multiple of that same tick, so it
  likely has a different source -- not yet identified. **Queued as a
  standalone `Time::getCurrentTime()`/pacer-timing probe** (isolated
  from the full game), not performed as part of this campaign -- two
  independent sessions (build-qa, vcctrl-c3) both converged on
  recommending this rather than guessing further from full-game data.
- Since real hardware confirms the pattern, `fps_p95=9.09` (not 15+) is
  arguably now the more honest reading of "the slow tail" than the
  original KPI story anticipated -- but a bimodal distribution's p95
  isn't the same shape as the single-tail-stall story `docs/BENCHMARK-
  PLAN.md` was originally written against, so treat this as a genuine
  new characterization finding to investigate, not yet a KPI number to
  compare across cards until the two-cluster mechanism is understood.
- Visual/audio not re-checked this run -- patch 0035 touches only the
  RUNMANIFEST capture site, no rendering/audio/input code, and both
  were already cleared twice on this same build family.
- Post-exit black screen / capture-freeze recurred a second time,
  resolved via camera as before -- third confirmed occurrence of the
  known item.

## Measurement method
Real hardware via vcctrl (authoritative), Gateway 2000 rig,
`gateway2000` profile. Second run this campaign (see
`cirrus-cl-gd5434-2026-09-02.md` for the first, different build) --
together these two runs are the first real repeat-run data this port
has for the Cirrus leg, though still not a full formal repeatability
study per `docs/hardware-testing.md`.
