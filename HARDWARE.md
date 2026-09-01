# Reference hardware

Every port should eventually be evaluated against the same hardware matrix,
so results are comparable across ports rather than each port inventing its
own baseline.

## Primary matrix

| ID | CPU | Role |
|---|---|---|
| HW-486-50 | 486DX2-50 | low-end torture test |
| HW-486-66 | 486DX2-66 | primary minimum target |
| HW-POD83 | Pentium OverDrive 83 | upgrade-path target |
| HW-5X86 | AMD Am5x86-133 | fast 486 platform |
| HW-P75 | Pentium 75 | recommended target |

For each machine, record when available: CPU, clock, L1/L2 cache, memory
size, chipset, video card, VESA BIOS/version, sound card, MIDI device, DOS
version, memory manager configuration. Cache configuration in particular
should never be omitted for a 486-class result — it materially changes
throughput.

## doskutsu's real-hardware baseline (for calibrating expectations)

| Hardware | Approximate render rate |
|---|---:|
| 486DX2-50 | ~19 FPS |
| 486DX2-66 | ~25 FPS |
| Pentium OverDrive 83 | ~33 FPS |
| Am5x86-133 | ~33 FPS |

Cave Story's game logic runs at a fixed simulation rate decoupled from
render rate — see `docs/timing.md`. A candidate does not need to hit
50/60 FPS to be playable if its simulation timing can be cleanly separated
from its render rate; use doskutsu's numbers as a rough complexity yardstick,
not a pass/fail bar.

## Known hardware-specific issues

A living log, updated as issues are found -- not just once resolved.
Record here as soon as a real-hardware-only symptom is localized to a
specific card/chip, even before root cause is known, so a second port
hitting the same card doesn't have to rediscover an investigation already
in progress. Once resolved and generalized, the durable lesson moves into
`docs/video.md`/`docs/audio.md` (cross-referenced below); this table
stays the fast index of what's known per component.

| Component | Status | Summary | Details |
|---|---|---|---|
| ATI Mach64 (215CT/-ET) | Resolved | Real-hardware-only video corruption on dossage/Passage at 640x480 (forced banked 24bpp, no LFB on this card) traced to a port-side bug: Passage's engine wrote raw pixels into the window surface assuming a fixed format instead of rendering into its own known-format surface and blit-converting at present time (see `docs/video.md`'s window-surface-format hazard). Fixed in dossage's own repo (its patch 0003, XRGB8888 render surface); confirmed resolved via a direct pixel-diff of a post-fix real-hardware capture against the DOSBox-X reference screenshot (2.85/255 mean difference, consistent with JPEG noise, not a defect). Several shared-layer hypotheses were investigated and ruled out clean along the way -- bank-switch chunk-content/readback (`SDL/0128`-`0131`), legacy VGA GR-register state (`SDL/0132`), CRTC offset register (`SDL/0133`), VBE 4F06 scan-line length (`SDL/0134`) -- all retained in `shared/` as hint-gated (zero-cost-when-unset), chip-agnostic diagnostic tooling for the next real-hardware-only video symptom, even though none of them were this bug. | `docs/video.md` (mode-selection/centering history, full investigation writeup); this table for current status |
| ATI Mach64 (215CT/-ET) | Resolved | Silicon cannot double-scan -- no 320x200/320x240 VESA mode exists at all (confirmed via UniVBE's own printed banner). Forces closest-mode-match to a larger mode, which is the trigger condition for the stride-caching bug above and for missing-centering/dirty-rect-coordinate bugs in doskutsu's own engine-side code. | `docs/video.md` |
| Cirrus CL-GD5430 (mislabeled "5434" throughout most of shared/'s own patch history -- HWiNFO misdetects it; physical silkscreen confirms 5430, no measurement invalidated) | Resolved | Genuine hardware defect: LFB aperture and the VRAM the display generator actually reads are decoupled under UniVBE 6.7. Fixed generically at the shared layer (`SDL/0019` auto-disables LFB on this chip; `SDL/0020` fixes a second-order BIOS/FB-layer disagreement `0019` itself introduced). | `docs/video.md` |
| S3 ViRGE (86C375 Rev B) | No known card-specific defect (confirmed by investigation, not just absence of evidence) | No stride/pitch or LFB-aperture issue found in doskutsu's history on this card (native 320x240 support avoids the Mach64 trigger condition). dossage/Passage's 486DX2-66 real-hardware campaign (2026-08-30/31) chased two symptoms on this card that looked like hardware defects and turned out not to be. (1) Launch-time intermittent corruption (~50-60% of launches, garbled/static-noise title screen -- process confirmed NOT hung, correctly blocked in the engine's normal input-wait with audio playing throughout; mode-set was byte-identical between garbled and clean runs) traced to a generic shared-layer bank-select timing gap (see the row below, and `docs/video.md`); fixed completely for this port by forcing LFB (`SDL_HINT_DOS_PREFER_LFB=1`, 5/5 clean vs. 3/5 garbled banked baseline). (2) A separate runtime progressive-corruption symptom that reproduced identically under banked AND forced-LFB and survived a power-cycle (ruling out the bank-select gap as its cause) -- a same-card doskutsu control run (3.5+ min clean) proved the card itself is fine; root cause not conclusively identified, resolved empirically -- see `docs/video.md`. Net real-hardware result: 486DX2-66 + this card runs dossage/Passage at 14.16-14.5fps, real audio, zero video corruption (dossage's own port-repo-local patches 0027-0029 -- distinct numbering from `shared/patches/sdl3-dos/`). The one remaining rig-wide (not card-specific) gotcha is UniVBE silently declining after a card swap -- see below. | `docs/video.md`, `dos-hardware-validation` |
| Banked-mode bank-select settle delay (`SwitchBank()` / mode-set banked-blank loop, all cards) | Open -- latent, unfixed in `shared/` | `SwitchBank()` (`SDL_dosframebuffer.c`) and the mode-set-time banked blank loop (`SDL_dosmodes.c`, a separate, textually-duplicated far-call/`INT 10h AX=4F05` implementation) have zero settle delay or readback verification at either call site, and every frame forces a fresh hardware bank-switch regardless of what the previous frame left selected (`current_bank` resets to a fresh `-1` per flush call). Found via dossage/Passage on a real S3 ViRGE 86C375 Rev B (see row above). Physically plausible mechanism (not independently proven): a posted-I/O-write race -- the chip acknowledges the bank-select write before it's actually latched, so a write burst starting immediately after can land in the *previous* bank, which would explain corruption that accumulates rather than flickers. There's a textual precedent for "a bank-switched write doesn't land where it should" in this codebase's own history (`shared/patches/sdl3-dos/0029`, verified: a Cirrus missing-bottom-strip regression), though that instance's cause was a code-generation/modular-arithmetic issue at the call site, not a hardware timing race -- the symptom shape isn't unprecedented here even though the mechanism differs. Worked around for dossage/Passage by forcing LFB (avoids the banked path entirely), not fixed at the shared layer -- the gap itself is still live for any port/card that takes the banked path. The diagnostic delay primitive a fix would reuse (`SDL_HINT_DOS_BANK_CHUNK_PROBE_DELAY`, patch `0131`) already exists but is wired only into a diagnostic readback, not the production write path -- wiring it into `SwitchBank()` with a bounded readback-verify retry is an open fix candidate needing its own review before landing. | `docs/video.md` |
| Any card, after a swap | Recurring operational hazard, not a one-time bug | `UNIVBE.EXE` prints nothing when it declines to install, so a stale `UNIVBE.DRV` from the previous card silently falls back to bare ROM VBE. Reads exactly like a hang. Check `MEM /C \| FIND "UNIVBE"` (positive-only, high-memory loads can false-negative) and the SDL boot log's `oem_string=` field (zero-cost, cannot-fabricate) before trusting any video diagnosis after a swap. | `shared/skills/dos-hardware-validation/SKILL.md` |
| Any card, vcctrl `power cycle` recovery | Unresolved, recurring operational hazard | Hit twice in one session recovering dossage/Passage on the S3 ViRGE rig (2026-08-30/31) -- once escaping an in-game garbled/blocked state, once during a routine cold-boot-timing test. Both times the machine came back from `vcctrl_power cycle` to an extended black screen with no POST progress (PS/2 link confirmed alive) -- once needed the operator's physical intervention, once self-resolved before intervention was needed. Not root-caused; not yet reproduced deliberately or characterized further. This is a rig/vcctrl-operational fact, not a video-hardware fact -- record here until it's understood well enough to move into a dedicated rig-operations doc/skill. | dossage's own `PLAN.md` ("Phase 0/2 result", 2026-08-31) |
| Startup (first framebuffer flush after SDL_Init), all cards on 486-class CPUs | Real, reproducible, not fixed -- flagged not chased | dossage/Passage's `DOS_Yield()` diagnostic (`shared/patches/sdl3-dos/0136`, temporary) found a one-time 50-70ms spike in the first ~200 frames only, correlating with the first framebuffer flush (first-ever DPMI mapping) and large enough to trip `SDL_DOSAudioPump()`'s runaway-caller safety cap (`shared/patches/sdl3-dos/0067`). Generalizable to any port using this backend's frame-limiter pattern, not dossage-specific -- see `docs/timing.md`. | `docs/timing.md` |
| Banked-mode flush, mid-run multi-hundred-ms stall (Cirrus CL-GD5430, dossage/Passage, 486DX2-66) | Real, reproduced, root cause not confirmed -- narrowed to one variable, campaign closed before rig time to settle it | Three-way real-hardware bisection of the render/flip span found `presentScreen` -> `SDL_UpdateWindowSurfaceRects` (`DOSVESA_UpdateWindowFramebuffer`) spiking from a normal ~2.2ms to 243-326ms (a 110-150x excursion) 3-4 times per run, at frame positions that reproduce across runs and are independent of player input; `worldrender`/`blowup` (the port's own render stages) stay in their ordinary range on the same frames -- the cost is inside the shared backend's flush, not the port's own code. `WaitForVBlank()` is called whenever `vsync_interval > 0 \|\| dac_needs_update`. Two legs, checked against real data rather than assumed: **(1) `dac_needs_update`, gated on `SDL_PIXELFORMAT_INDEX8` + a changed palette version, is structurally ruled out for dossage specifically** -- confirmed 16bpp (`320x240 16bpp` in the mode-set log; game.cpp's own fast path is gated on `bytes_per_pixel == 2\|3\|4`), so this port never presents INDEX8 and the leg can never fire here. Still real and worth checking for any future *paletted* port on this backend -- a scripted fade/palette-cycle at fixed narrative beats would explain a reproducible-position-independent-of-input stall exactly this way. **(2) `vsync_interval > 0` is unconditionally true for dossage** -- vsync is explicitly armed (`SDL_SetWindowSurfaceVSync(window, 1)`, game.cpp:584), so `WaitForVBlank()` runs on every single flush, not just special frames. This fits the measured bimodal shape (either ~2.2ms or 243-326ms, nothing between) better than the palette theory: `WaitForVBlank()` (patch `SDL/0116`, "VBLANK-BOUND", default ON) is two back-to-back bounded spins on the VGA status port, each capped at 2,000,000 iterations, explicitly designed so "a mode whose retrace bit never toggles yields a finite stall + fall-through instead of an infinite hang" -- the guard is doing exactly its documented job; **the open question is upstream of it, whatever makes this Cirrus's retrace bit undetectable on those specific frames.** One-variable test to settle it, not yet run: instrument the guard-loop's actual iteration count on stall vs. normal frames -- pegged near 2,000,000 on the 3-4 stall frames and low elsewhere confirms it. Generalizable either way: any port landing in banked mode on this backend, paletted or not, could hit a mysterious multi-hundred-ms frame with no obvious cause in its own code -- took eight falsified/narrowed hypotheses on dossage's side to get this far. | `HARDWARE.md`, `SDL_dosframebuffer.c` (`WaitForVBlank`, `dac_needs_update`) |
| Any card, black screen ~20s after ESC/exit (Cirrus, dossage/Passage, 486DX2-66) | Real, reproduced once, not chased -- unrelated to fps | vcctrl observed a black screen for roughly 20 seconds after the game exited via ESC, PS/2 link responsive throughout and a subsequent reboot normal -- not a hang. Looks like slow video-mode teardown on exit, not investigated further since it doesn't affect the fps KPI. Record here so the next port that hits a similar post-exit delay doesn't start from zero. | dossage's own campaign notes, 2026-08-31 |

## Real-hardware testing process

Real-hardware validation for all ports currently runs through
[vcctrl](https://github.com/ecliptik/vcctrl), a KVM/automation rig (PS/2
input injection, VGA/audio capture, power control, FTP file transfer) built
around one physical machine with hot-swappable CPUs matching the matrix
above. See [`docs/hardware-testing.md`](docs/hardware-testing.md) for the
integration design and current workflow, and
[`templates/vcctrl-profile.yaml.template`](templates/vcctrl-profile.yaml.template)
for how a new port defines its own vcctrl profile.

DOSBox-X (and, for a second correctness opinion, 86Box) automation is the
day-to-day regression gate; real hardware is the authoritative result for
performance and device-compatibility claims. Never report a performance
number measured only in an emulator as if it were a hardware result.
