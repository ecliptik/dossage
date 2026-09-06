# Rig runbook: driving the Gateway 2000 for a benchmark/validation campaign

Written 2026-09-02 by the `vcctrl` peer session, at the request of this
repo's `docs/BENCHMARK-PLAN.md` ("Team shape" section), after a mid-campaign
`/clear` in an earlier session was found to have wiped all procedural
know-how while `PLAN.md` kept only the findings. This is the doc that gap
argued for: the operational mechanics an agent driving this rig actually
needs, not the theory. Homed in *this* repo (not the `vcctrl` repo) because
it is dossage-campaign-specific; it points into the `vcctrl` repo's own docs
(marked below) for the generic rig mechanics rather than repeating them.

If you are about to drive this rig for the first time in a session, read
this top to bottom before the first `vcctrl_*` call. It assumes the
`vcctrl-mcp-workflows`, `vcctrl-common-workflows`, `vcctrl-rig-hazards`, and
`vcctrl-dinspect-sysinfo` skills — load those too; this doc doesn't repeat
their content, only sequences it.

## 0. Orient before touching anything

- `vcctrl_profiles()` — confirm which profiles are actually running. This
  rig has carried `gateway2000` alone and `gateway2000` + `jezebel` on one
  daemon at different times; don't assume which from a stale memory.
- `vcctrl_activity()` — who holds the input lock, what's in flight. Refuse
  to proceed past a held lock or a running job you didn't start; see
  `vcctrl-mcp-workflows` for the lock-recovery discipline.
- `vcctrl_note(text)` — say what you're about to do, once, before the first
  action. It's the only thing that puts context on the KVM status bar for a
  human watching, and it costs nothing.
- `vcctrl_shot()` — one frame, no lock, no reboot. Confirms capture is
  locked and shows what boot profile the machine is actually sitting in
  *right now*, before you assume anything from a plan doc or a previous
  session's memory.

## 1. The identity trap: a card swap is invisible to the daemon

**The daemon cannot detect a video card, sound card, or CPU change on its
own.** It can only see a USB4VC *protocol board* change (`vcctrl_board`),
which is a different thing from what's plugged into the PCI slot. This means
`vcctrl_sysinfo()` can report `stale: false` and still describe hardware
that was swapped out days ago — `stale` here means only "no board change
since this reading," not "nothing changed."

**Confirmed live, 2026-09-02:** the cached dinspect reading was 57+ hours
old and reported `Video Chipset: Cirrus Logic (device 00A8)`, `Video Memory:
1 MB` — this is the Cirrus GD-5434 from the 2026-08-31 swap documented in
`VIDEO-SWAP.md` (vcctrl repo), not whatever card is fitted now. If a card
swap happened after the last scan (check `age_s` against when the swap is
supposed to have happened, not against "recently"), **do not trust
`vcctrl_sysinfo`'s CPU/video/VRAM fields until a fresh scan runs.** `PicoGUS`
and other non-video fields are unaffected by a card swap and can still be
read from the cached copy, but say so explicitly rather than presenting the
whole block as current.

Getting a fresh reading (`vcctrl_file_scan(mode="return", confirm="scan")`,
poll `vcctrl_file_status()`, then re-read `vcctrl_sysinfo()`) reboots the
target twice — see `vcctrl-dinspect-sysinfo` for the full sequence and its
precondition (DINSPECT.EXE must already be staged at `C:\XFER\IN`). Treat
this as an input/power action requiring the same go-ahead any other reboot
does; it is not implied by "check current hardware."

**UniVBE's own `oem_string`/`oem_vendor` is not a card identity witness.**
Confirmed in `VIDEO-SWAP.md` (vcctrl repo): UniVBE *shims* the VBE identity
and reports `SciTech Software, Inc.` / `Universal VESA VBE 6.70` on every
card it's configured for, ViRGE or Mach64 or Cirrus alike. Asking it "what
card is this" returns a constant that looks like a reading and isn't one.
It is still the right field to confirm UniVBE itself (a real VBE 2.0+
provider) is active rather than a bare ROM VBE 1.2 fallback — that's a
different question than card identity, and `docs/BENCHMARK-PLAN.md`'s own
identity-witness step is asking that question, not "which chip is this."
Two tools exist specifically for the chip-identity question:

- `bin/vcctrl-cardid <sweep>SDL.LOG` (vcctrl repo) — identifies the card
  from *behaviour* (mode list, framebuffer address, VRAM size, the engine's
  own detect probes) rather than asking VBE. Says `UNKNOWN` rather than
  guessing on a thin log.
- `dinspect`'s `Video Chipset` field reads the chipset a different way (not
  through UniVBE) and is the field to trust for "what chip is this,"
  distinct from the `Video` field's VBE-version string.

So: **"UVCONFIG has been re-run since the swap" is necessary but not
sufficient to know which card is fitted** — re-running UVCONFIG makes the
VBE layer functional on whatever's there, it does not identify it. Use
`cardid` against an actual run's SDL log, or dinspect's `Video Chipset`
field from a *fresh* scan, not UniVBE's banner.

## 2. Video card swap procedure (operator does the physical swap)

Full procedure and per-card history: `docs/VIDEO-SWAP.md` (vcctrl repo).
The short version, because the failure modes are non-obvious:

1. Operator swaps the card and powers on (or tells the harness to — cold
   boot needs the LED edge-pair wait, not a level check, see
   `docs/TIMING-FIXES.md`, vcctrl repo, bug 3).
2. Confirm capture still locks — grab a frame at the DOS prompt before
   anything else. A new video BIOS is the most likely thing to break the
   capture path.
3. `bin/vcctrl-cardid` against a sweep log to identify the card from
   behaviour, not from UniVBE. **Don't trust this run alone if UVCONFIG
   hasn't been re-run yet for the new card** — confirmed 2026-09-03 on a
   Cirrus→ViRGE swap: cardid came back LOW CONFIDENCE with a stale driver
   still in place, top match "Cirrus (onboard)" (4/5=80%) narrowly beating
   ViRGE (6/8=75%), both `s3_probe` and `cirrus_bug` signals firing at
   once, and `total_vram` reading 1024 KB — matching the *previous* card,
   not the one physically installed. Fresh `dinspect` agreed with the
   physical swap (`S3 ViRGE/DX or /GX`) even at this stage, so the two
   witnesses can genuinely disagree here; re-run cardid after step 4/6
   below rather than trusting a pre-UVCONFIG read of either one alone.
   Also watch for a physical "OUT OF RANGE" on the monitor itself (not
   just a capture-stick freeze) during a cell's gameplay mode-set with a
   stale driver in place — same root cause, worse symptom, resolved the
   same way (reboot picks a safe mode again; it's the driver that's stale,
   not the hardware).
4. `bin/vcctrl-uvconfig` — **as of 2026-09-03 this tool REFUSES to run
   UVCONFIG at all**, and that refusal is correct, not a bug to route
   around. UVCONFIG.EXE only renders in text mode (`MODE03`), but this
   rig's VGA capture stick only locks onto mode 12h — so the instant the
   machine switches to text mode for UVCONFIG, the harness (and this
   session, capture-only) goes completely blind to the screen for the
   entire interactive portion. The tool won't start something it cannot
   see through to a safe finish; its own refusal message cites the
   six-hour incident this exact blind-interactive shape caused before.
   **This step now requires a human physically at the machine**:
   ```
   C:\VGACAP\MODE03          (text mode -- capture goes blind, expected)
   C:\UNIVBE\UVCONFIG.EXE    (read what it prints: chip detected, any
                               withheld modes -- the Mach64-CT case that
                               started this whole procedure)
   C:\VGACAP\MODE12          (restores capture)
   Ctrl-Alt-Delete            (harness can do this part — AUTOEXEC loads
                               the new config on boot)
   ```
   After the reboot, `vcctrl-uvconfig --verify` doesn't check anything
   itself — it prints `FIND`-based instructions for grepping a fresh
   cell's SDL log for `oem_string`, the expected mode ID, `LFB-decision`,
   and `total_vram` (which identifies the card through the shim: 2048 KB
   Mach64, 4096 KB ViRGE — a very different number from cardid's *direct*
   `vram=` reading in the same log, and both are worth checking). Earlier
   text in this repo's history described this tool as running UVCONFIG
   non-interactively and closed-loop — that was true for an older version;
   it no longer runs UVCONFIG under any circumstance as of this date.
5. **If the VGA capture stick loses lock (`state: "frozen"`) mid-run, that
   is not the same as a stuck interactive menu.** Pull a frame from the
   hardware camera (`vcctrl_camera_shot`, `vcctrl-camera` skill) before
   concluding anything — this has resolved to "already back at a clean
   prompt" twice (Cirrus 2026-08-31, and see that entry in `VIDEO-SWAP.md`)
   when the analog capture alone was ambiguous.
6. Reboot (uvconfig generates a driver file; the TSR that reads it only
   does so at load time, so a stale one stays resident until reboot),
   confirm the prompt via RDYPULSE.
7. ~~Run the anchor sweep (RB, `--collect`).~~ **NOT a default step —
   operator correction, 2026-09-03**: *"Card swaps should be relatively
   lightweight, not full regression tests every time."* Fresh identity
   witnesses (dinspect + cardid, both re-run after step 4/6 above, not
   before — see step 3's note) are the swap validation. A full sweep is
   something a *benchmark* run decides to do, not something every swap
   owes by default — running one to "validate" a swap that dinspect+cardid
   had already confirmed was the over-testing this correction is about.
8. **A swap starts a new results column.** Say so explicitly when handing
   numbers to an analysis session, or the comparison happens by default.

## 2a. Structuring a multi-CPU × multi-card campaign

**The card is the expensive, outer loop; the CPU is the cheap, inner
loop — swap the campaign around that asymmetry, not around convenience.**
A card swap costs a physical operator (the UVCONFIG step above) plus a
full identity re-confirmation; a CPU swap is a plain hardware swap with no
driver state to regenerate downstream. Running UVCONFIG once per card and
sweeping every CPU against it, versus once per card/CPU pair, is the same
coverage for a fraction of the physically-present-operator time:

    for each card (ViRGE, Mach64, Cirrus):
        swap the card, run UVCONFIG once (section 2, steps 1-6)
        confirm identity once (fresh dinspect + cardid)
        for each CPU (POD-83, Am5x86-133, DX2-66, DX2-50):
            swap the CPU (cheap, no UVCONFIG)
            run that CPU's benchmark cell/sweep

Three UVCONFIG runs total instead of twelve, for a 3-card × 4-CPU matrix.

## 3. PicoGUS mode

Confirmed live 2026-09-02 from the boot banner: this rig's `PGSB` profile
boots to `picogus-sb-dbop13 v4.1.1` firmware, `Running in Sound Blaster 2.0
mode on port 220, IRQ 7, DMA 3`, `AdLib port 388`. `dinspect`'s `PicoGUS`
field (`SB mode (PicoGUS 2, protocol v4)`) is not video-card-dependent and
stays trustworthy even when the video fields in the same reading are stale.

The three PicoGUS boot entries (`PGSB`/`PGADLIB`/`PGGUS`) differ only in
`AUTOEXEC.BAT` environment and a `pgusinit /mode ...` call — see
`docs/PICOGUS-CONSOLIDATION.md` (vcctrl repo) for the full mapping and why
mid-session mode switching (`PUMP.BAT` already does it) is not speculative.
The thing that bit this project before: a leftover env var from one profile
contaminating another after a switch that set new variables without
clearing old ones (`DEEP.BAT`'s comment about cells that "worked anyway by
INHERITING SB mode"). If a manifest declares `config=%config%` rather than
reading `pgusinit`'s own no-argument mode report, that's a declaration of
intent, not a reading of hardware — worth knowing which a given sweep's
manifest actually does before trusting it as a card-mode witness.

## 4. Staging a build and launching it

Standard upload recipe (`vcctrl-common-workflows`, "Uploading a file to the
target"): `vcctrl_file_check()` -> `vcctrl_stage_file(path, name=...)` ->
`vcctrl_file_queue("list")` to confirm what's actually queued ->
`vcctrl_send_file(mode, confirm="send")` -> poll `vcctrl_file_status()`.
`mode="return"` reboots to the menu default afterward; `mode="stay"` leaves
the machine in the NET profile, which is not a valid state to start a
measured run from.

A destination subdirectory that doesn't exist yet on the card needs its
full path pre-created by hand first (`MD` one level at a time — DOS 6.22's
`MD` can't create nested paths in one shot) — `send_file`'s target batch
only auto-creates the deepest level of `dest`, and everything staged in one
call lands flat under that single `dest` with no per-file subdirectory
support.

`docs/BENCHMARK-PLAN.md`'s own pre-flight also requires hash-verifying the
staged binary before staging *and* after sending, and confirming
`./scripts/verify-patches-applied.sh` exits 0 for the exact build being
staged — both are this repo's own gate, not a vcctrl mechanic, but belong
in the same round as the steps above.

**Launch DOSSAGE.EXE with its stdout redirected to a file, every time —
a bare `DOSSAGE` at the prompt loses the primary metric.** Confirmed
2026-09-04 (ViRGE + 486DX2-50 leg): `game.cpp`'s
`printf("Frame rate = ... fps (%d frames)\n", ...)` — the "Average FPS"
line every benchmark doc in this campaign leads with — only goes to the
DOS console device, never to a file, unless the launch command redirects
it. DOS text mode has no scrollback, and the CWSDPMI exit-stats banner
prints right after that line and scrolls it off-screen, so a missed
redirect is not recoverable after the fact — it costs a full re-run.
RUNMANIFEST's `fps_p50`/`fps_p95` are unaffected (that write goes to a
file regardless), but they alone can't score a leg against the
average-fps KPI band. Launch as `DOSSAGE > FPS.TXT` (or any filename
that doesn't collide with a stale file left over from an earlier launch
this session — the Am5x86-133 leg's own Notes record a
`FPUSTDO.TXT`/`RUNSTDOU.TXT` naming mixup from exactly that).

**A destination directory name typo lands you in the wrong game
entirely, and it plays as a crash, not an error.** Confirmed 2026-09-04:
staging to `C:\DOSKUTSU` instead of `C:\DOSSAGE` (misreading the boot
banner's own unrelated-QA-game instructions) launched cleanly as far as
DOS is concerned, then `DOSSAGE.EXE` page-faulted immediately in that
foreign environment — nothing about the fault pointed at the actual
cause. If a launch that staged clean crashes instantly, check `CD` /
the prompt path before suspecting the build. Recovery is a plain `DIR`
-> delete-what-you-added -> re-stage-to-the-right-directory -> re-`DIR`
to confirm the original tree is intact; if the two DOS ports on this rig
happen to vendor the identical upstream CWSDPMI build (they do, per
doskutsu's own `THIRD-PARTY.md`), overwriting one game's `CWSDPMI.EXE`/
`.DOC` with the other's copy of the same file during that mixup is a
no-op, not a corruption — still worth a note to whichever session finds
the timestamp changed later.

## 5. Capture-mode practice while a run is going

- Prefer `vcctrl_burst` over `vcctrl_shot` to confirm what just landed on
  screen — `shot()`'s judged frame has measured up to ~13s stale against a
  fast-changing screen; a raw burst is fresher.
- Don't re-burst every 30-40s just to watch a slow-developing state (a
  corruption bug progressing, a long non-interactive step). The capture
  ring already records continuously; pull `vcctrl_timeline()` plus a couple
  of `vcctrl_frame(seq)` samples after the stretch instead of polling live.
  Save a live burst for an actual decision point.
- A video health check is not an audio health check — they're separate
  capture pipelines. `docs/BENCHMARK-PLAN.md` says to fold `vcctrl_audio_verdict`
  into mid-run health checks, not just video — don't assume a clean frame
  covers it.
- `vcctrl_verify_input`'s LED round trip proves the PS/2 link is alive; it
  does not prove any particular text rendered. Use it for "is the target
  responsive at all," a burst for "what does the screen actually show."

## 6. DOSSAGE launch lessons from the Phase 2 campaign (2026-09-04)

Learned the hard way during the DX2-50 Rounds 1-3 fix-validation campaign
(`docs/BENCHMARK-PLAN.md`, `docs/benchmarks/mach64-215ct-486dx2-50-round{1,2,3}-2026-09-04.md`)
-- DOSSAGE-specific, not generic vcctrl mechanics, which is why this lives
here rather than in the vcctrl repo's own docs.

- **A cold boot means an empty DOS environment.**
  `SET SDL_HINT_DOS_FORCE_MODE_ID=0x0111` does not persist across a power
  cycle -- it must be re-typed before the *first* Mach64 launch of a
  session, every time. Skipping it once let the Mach64 negotiate its
  unforced default (512x384), which produces two symptoms that are easy
  to misread:
  - the primary VGA capture stick cannot lock this mode (reads
    `state: "frozen"`, judged as no picture) -- **this is a capture-lock
    failure, not a hang**;
  - the physical monitor shows a squashed strip across the top rows, not
    a full picture -- also not a hang, just an unvalidated video mode
    actually rendering.
  Combined with `vcctrl_verify_input`'s LED check failing (see below) and
  real power draw, this can look exactly like the RDTSC-wedge-under-
  EMM386 hang signature from the CPU-identity investigation. **Confirm
  with the operator directly (can they move the character?) before
  reaching for a power cycle** -- a false hang report cost a round-trip
  here.
- **The title screen is silent by design.** The game sets loudness to 0
  before the first title and only raises it once a key or event starts
  the life. Silence at the title is not evidence of an audio fault.
- **`vcctrl_verify_input`'s LED round-trip proves nothing while DOSSAGE/
  SDL owns the keyboard controller.** SDL grabs INT 9 directly during any
  run and does not toggle keyboard LEDs at all -- a failed LED check
  during a DOSSAGE run just means SDL is in control (expected), not that
  the PS/2 link is dead. This is stronger than section 5's general LED-
  check caveat above: during a DOSSAGE run specifically, the LED
  round-trip is not merely imprecise, it is not a valid test at all.
- **The first life can start on ANY key or joystick event, not
  necessarily the operator's intended keypress.**
  `waitForKeyOrButton()` returns on `SDL_EVENT_KEY_UP` as well as
  `KEY_DOWN` -- even the *release* of the Enter key used to launch
  `DOSSAGE.EXE` can start the life, if it arrives after SDL installs its
  keyboard handler. Every run's `STDOUT.TXT` reports `"Found 1
  joysticks"` -- a floating/unconnected gameport can also deliver a
  spurious button event at an unpredictable moment. Net effect:
  launch-to-title wall-clock brackets carry tens of seconds of slop
  (33-166s observed across this campaign) that is **not clock loss** --
  confirmed directly via an independent in-game CMOS RTC witness, which
  agreed with the engine's own clock to 0.0% across two separate real
  lives. Get the true life-start timestamp from the capture timeline
  (poll for the title-to-gameplay transition, or `vcctrl_pin` the ring
  before launch) rather than from the launch or title-reappear
  timestamp, if the life interval itself needs to be exact.
- **DOSSAGE's own exit sequence**: at the title screen, Escape counts as
  "a key" and starts a new life rather than exiting -- only an in-game
  Escape quits. Reliable two-step exit: press Escape once when the title
  reappears (starts a short life-2), wait ~3s, press Escape again
  (in-game -- clean exit). This leaves a short (<20s) trailing
  RUNMANIFEST block, expected and filterable by duration in analysis.
- **`build_sha12` path caveat**: recomputable via `make build-sha12`, but
  only from the exact worktree a build came from -- a feature-branch
  build made in an isolated worktree (e.g. `/home/claude/git/dossage-dx2-50`)
  will not necessarily reproduce the same hash if recomputed from a
  different checkout of the same branch/commit. Confirm which worktree a
  given `build_sha12` was generated in before treating a mismatch as a
  real discrepancy rather than a path artifact.

## 7. Cross-references

All of the following live in the `vcctrl` repo, not this one:

- `docs/VIDEO-SWAP.md` — full swap procedure, per-card results (ViRGE,
  Mach64, Cirrus GD-5434), the RB.BAT provenance-hole discussion.
- `docs/PICOGUS-CONSOLIDATION.md` — boot-profile collapse, env hygiene.
- `docs/DINSPECT-SYSINFO.md` — the one banked measurement of what a scan
  costs; `.agents/skills/vcctrl-dinspect-sysinfo` — the how-to (re-scan
  sequence and precondition, updating the vendored binary).
- `docs/MCP-SERVER.md` — full tool reference.
- `docs/TIMING-FIXES.md` — cold-boot LED edge-pair timing, RDYPULSE.
- `.agents/skills/vcctrl-mcp-workflows/SKILL.md`,
  `.agents/skills/vcctrl-common-workflows/SKILL.md`,
  `.agents/skills/vcctrl-rig-hazards/SKILL.md` — the discipline this doc
  assumes and doesn't repeat.

In this repo: `docs/BENCHMARK-PLAN.md` — the per-card procedure and KPI
bands this runbook exists to support.
