# Hardware testing (vcctrl integration)

This documents how real-hardware validation should work across multiple
ports. It is a **design**, not yet implemented automation — the harness
scripts it describes are still doskutsu-coupled today (see "Current gap"
below). Nothing in `vcctrl` is modified as part of establishing this
document; generalizing the harness is tracked as future work.

## What vcctrl is

[vcctrl](https://github.com/ecliptik/vcctrl) is a remote-control/automation
harness for a physical vintage PC, driven by a CLI, an MCP server (for
agent control), or a browser KVM. It provides:

- **Keyboard/mouse input** injected over PS/2 via a USB4VC bridge, so an
  agent or script can drive the DOS machine as if typing at the console.
- **Screen capture** via a USB VGA capture stick, with a scrub-ring buffer
  (still/burst/timed recording).
- **Audio capture**, historically a coarse liveness signal (silence vs.
  music) via raw level alone, not a quality judgment. Now also exposes
  frequency-aware content detection (below) that distinguishes real
  music/SFX from a steady tone/hum at the same volume — level alone
  can't tell those apart, since it only proves *something* is on the
  wire.
- **Power control** via a smart plug, scoped so a power action can't hit
  the wrong machine.
- **File transfer** over FTP, always initiated from the DOS side via mTCP
  (nothing is pushed to the target unsolicited).
- A **DOS liveness signal** using the PS/2 keyboard LED return channel,
  since there is no serial console to poll.

Today it targets one physical rig (one machine, hand-swapped CPUs tracked
as machine IDs matching this repo's `HARDWARE.md` matrix — 486DX2-50,
486DX2-66, Pentium OverDrive 83, Am5x86-133).

## Config layers (already generic, reuse as-is)

- **`vcctrl.yaml`** — one per **physical rig** (gitignored, lives on the
  control host, not tracked in any repo). Declares the capability backends
  (input/leds/power/video/audio/files/board) and a `harness.profile` key
  pointing at which project profile is active.
- **`profiles/<name>.yaml`** — one per **software project**, tracked in
  that project's own repo (or, for now, in vcctrl's `profiles/` directory —
  see "Current gap"). Holds project-specific facts: DOS working directory,
  env-var names the program reads (TAS replay, PRNG seed, log verbosity),
  boot-menu timing, the machine/CPU table for that rig, per-sweep wall-clock
  budgets.

This split already maps cleanly to a multi-port world: **one rig config,
many project profiles**, switched via `harness.profile`. A new port defines
its own profile using `templates/vcctrl-profile.yaml.template` (modeled on
vcctrl's `profiles/doskutsu.yaml`) and does not need to touch `vcctrl.yaml`.

## Current gap: the orchestration layer is doskutsu-coupled

`harness/vcctrl-sweep`, `-cell`, and `-collect` (vcctrl's unattended
run/log-collection scripts) and a few `bin/vcctrl-*` tools (`vcctrl-cardid`,
`vcctrl-score-pump`, etc.) currently hardcode `C:\DOSKUTSU\LOGS` paths and
`DOSKUTSU_*` env var names. A second port cannot run an unattended sweep
through these scripts without either generalizing them or forking a
per-project copy. `vcctrl`'s own `docs/HARNESS-STANDARD.md` already
anticipates this — it's explicitly written target-agnostic and states it
"will move to a repository of its own when a second project adopts it."
That move is the natural trigger once a second port reaches real-hardware
QA; it is out of scope for this repo's current milestone.
`shared/skills/dos-hardware-validation/` (below) documents the discipline
a campaign should follow regardless of whether the harness scripts
themselves are generalized yet — it doesn't resolve this gap, it's a
layer on top of the manual interim workflow.

## Interim workflow (manual, works today for any port)

Until the harness layer is generalized, any port can validate on real
hardware using vcctrl's generic file-transfer primitives directly:

```sh
vcctrl file-check                    # confirm the Pi's FTP server is up
vcctrl stage-file <PORT>.EXE         # sha256, DOS-8.3-rename, queue
vcctrl send-file --return            # reboot into NET profile, pull + verify, reboot back
vcctrl file-status                   # poll transfer completion

# ... exercise the game on the target, via the browser KVM or scripted
# keystrokes over the CLI/MCP layer ...

vcctrl file-refresh --return         # reboot, refresh target's OUT listing
vcctrl file-list
vcctrl get-file <RESULT> --return    # fetch a result/log/save file back
```

Screenshots/video: `vcctrl shot` / `lastgood` / `frame` / `burst` / `record`.

Audio liveness: `vcctrl level` (raw dB, tells you *something* is on the
wire, nothing about what). For content-aware checks, vcctrl now also
exposes frequency-based MCP tools — no human listener required to confirm
music/SFX is actually playing:

- `vcctrl_audio_verdict(ms=3000)` — the default reach-for-first tool.
  Answers NO_SIGNAL / SILENT / AUDIO_PRESENT, plus `tone_like` (true =
  steady tone/hum, false = real content, null = couldn't tell).
- `vcctrl_audio_match(file_path, ms=3000)` — control-mode only. Compares
  the live signal's frequency shape against a local reference audio file
  (a port's own music/SFX asset) and returns a similarity score. **Coarse
  by construction** — 8 bands can't distinguish two tracks with similar
  broad frequency balance, verifies nothing about tempo/melody/timing, and
  has no calibrated match/no-match threshold yet. Read it as "does the
  live signal's frequency balance resemble the reference's," never as
  "this exact track is playing right now."
- `vcctrl_spectrum(ms=3000)` — the raw 8-band (100Hz-12.8kHz octave-spaced)
  numbers underneath both of the above, plus `active_bands`. Compares
  bands to each other rather than to an absolute level, so it stays
  meaningful regardless of where a volume knob sits in the signal path.

Full detail/caveats: `vcctrl-common-workflows` skill, "Checking audio."
One sequencing trap worth knowing up front: on a title-screen-gated game, a
SILENT verdict (or a low match score) may just mean the start-music
keypress hasn't landed yet, not that audio is broken — confirm the
keypress registered (a visible on-screen change) before concluding a
fault. The underlying measurement is FFT-based (a per-band Hz *range*
sum) — an earlier single-frequency Goertzel-probe version passed every
synthetic-tone test but read real captured game music as complete
silence, because a multi-second analysis window has sub-Hz resolution and
real music essentially never sits on an exact probe frequency. If a port
ever extends this measurement further, validate against real captured
rig audio, not just synthetic tones, before trusting a new result (see
vcctrl's own `docs/FINDINGS.md` sec. 42 for the full writeup).

## What a port should record

Follow `templates/BENCHMARK.md`'s format, and vcctrl's own result-envelope
discipline (`docs/HARNESS-STANDARD.md` in the vcctrl repo): declared +
detected hardware, a captured (not inferred) completion witness, and a
statistical-repeatability approach for FPS claims rather than a single raw
number — across-session FPS noise on real hardware has been measured at
several times the within-session band, so a single run is not a result.

## Validation discipline: `shared/skills/dos-hardware-validation/`

Beyond the manual command sequence above, `shared/skills/dos-hardware-validation/`
is a genericized Claude Code skill capturing the discipline a real
validation campaign needs to produce a trustworthy result: design a
comparison and verify it against source before running anything, populate
with an explicit destination + sha256 verify (never trust a transfer
capability's default inbox), per-cell `set`/`forbid`/`expect_log`
three-witness discipline, RUNMANIFEST-driven metric extraction, ABBA
methodology with pre-registered thresholds, treating a harness bug as a
bug to fix (not a workaround), and handing off raw data instead of a
narrated summary. Distilled from a real doskutsu campaign on the vcctrl
rig — see `shared/skills/README.md` for provenance and adoption (it
reaches every port as `/sdldos:dos-hardware-validation` through the
hub's `sdldos` plugin).

`shared/skills/dos-rig-operations/` covers the same rig at a lower level
— input injection, screen capture, file transfer, log collection, and
power management for day-to-day work, independent of running a full
validation campaign (staging a build to poke at manually, pulling one log
back, etc.). `dos-hardware-validation` assumes this skill's discipline
rather than re-deriving it. See `shared/skills/README.md` for the full
skill list, including the build/QA-side companion
`dos-realhw-verification`.

## Future work (explicitly not this milestone)

- Generalize `harness/vcctrl-sweep|-cell|-collect` to read the active
  project's env-var names, paths, and cell/BAT naming from its profile
  instead of hardcoding doskutsu's.
- Move `docs/HARNESS-STANDARD.md` out of vcctrl into a shared location once
  a second project adopts it (per vcctrl's own stated intent).
- Decide whether multiple physical rigs eventually need a registry, or
  whether one rig with swappable CPUs/boards remains sufficient for the
  matrix in `HARDWARE.md`.
