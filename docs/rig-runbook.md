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
   behaviour, not from UniVBE.
4. `bin/vcctrl-uvconfig` — runs UniVBE's `uvconfig`, then **looks** rather
   than sending a fixed key sequence. On every card tested so far (ViRGE
   baseline, ViRGE->Mach64, ViRGE->Cirrus GD-5434) it has run fully
   non-interactively and returned straight to a prompt in ~2 seconds. If a
   screen appears instead, that is the swap path working as intended, not a
   failure — capture between every keystroke by hand from there, never send
   a guessed pair. See `vcctrl-uvconfig`'s own docstring for why: the
   remembered "press space, press space" workflow has been tested against
   three separate swaps and never reproduced.
5. **If the VGA capture stick loses lock (`state: "frozen"`) mid-run, that
   is not the same as a stuck interactive menu.** Pull a frame from the
   hardware camera (`vcctrl_camera_shot`, `vcctrl-camera` skill) before
   concluding anything — this has resolved to "already back at a clean
   prompt" twice (Cirrus 2026-08-31, and see that entry in `VIDEO-SWAP.md`)
   when the analog capture alone was ambiguous.
6. Reboot (uvconfig generates a driver file; the TSR that reads it only
   does so at load time, so a stale one stays resident until reboot),
   confirm the prompt via RDYPULSE.
7. Run the anchor sweep (RB, `--collect`) — not expecting it to match the
   old card's numbers, but because it has the most banked history and is
   the most interpretable shape.
8. **A swap starts a new results column.** Say so explicitly when handing
   numbers to an analysis session, or the comparison happens by default.

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

## 6. Cross-references

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
