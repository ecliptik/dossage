# Audio

## Hardware backends already proven (doskutsu, inherited via `shared/`)

- Sound Blaster 16 compatible PCM (DMA).
- OPL2 / OPL3 FM synthesis.
- MPU-401 / General MIDI / WaveBlaster.
- Gravis UltraSound / PicoGUS.

`shared/audio/midi_sched.{c,h}` is the canonical SMF (Standard MIDI File)
parser + tick scheduler — backend-agnostic via a sink callback struct
(note on/off, CC, program change). Do not write a second MIDI scheduler
per port. The per-device hardware backends themselves (OPL2/OPL3, GUS)
are not separate `shared/audio/` files today — the actual device-specific
logic lives in `shared/patches/sdl3-dos/` (e.g. `0112`-`0114` for native
GUS/GF1 support) as part of the SDL3-DOS backend, not as standalone
engine-side backend source a port links against directly.

## Decode cost vs. render cost

Music decoding can be more expensive than game rendering on a 486. Plan for
multiple tiers rather than one fixed audio pipeline:

```
Pentium-class:  decoded digital soundtrack (e.g. OGG/Vorbis if affordable)
486:            lower-rate preconverted audio, OR MIDI arrangement,
                OR an optional no-music performance mode
```

Independent confirmation from an unrelated engine on the same platform:
[DevilutionX's own DOS build notes](https://github.com/diasurgical/DevilutionX/blob/master/docs/building.md)
record that real-time MP3 decoding drops their game to ~4.5fps on DOS
(their `dr_mp3` decoder compiles and runs, it's just too slow), and their
documented fix is the same shape as above — ship WAV-only audio in the
DOS build's data files rather than attempting compressed-audio decode at
all.

Hardware MIDI is the highest-leverage option on slow hardware: moving music
playback to an external synthesizer removes essentially all CPU cost from
the host. If a game supports MIDI at all, hardware MIDI should be the
recommended 486 configuration.

## Phasing a new port's audio work

1. **PCM only** — WAV/SFX via Sound Blaster, no compressed audio.
2. **MIDI** — OPL3, MPU-401/WaveBlaster, GUS/PicoGUS via `shared/audio/`.
3. **Compressed audio** (OGG/Vorbis, etc.) only if profiling shows the CPU
   budget allows it; provide a lower-rate transcoded fallback for slow CPUs.
4. Additional formats only when a specific high-value game requires them.

Never let compressed-audio feature completeness degrade 486 performance —
it should always be an opt-in tier, not the default path.

## Real-hardware audio hazards (from doskutsu's own campaign)

Sourced from a long real-hardware campaign across SB16/Vibra16,
WaveBlaster/MIDI, AdLib OPL2, OPL3, and GUS/PicoGUS — cite `dosskutsu` for
exact patch numbers/code locations if a claim below needs deeper
verification than what's noted here. Organized architecture-wide first
(relevant to any port using this shared backend), then per-device.

### Architecture-wide

- **Cooperative-scheduler audio starvation is a real, hard floor, not a
  bug to chase.** Below roughly 25 fps wall-clock on this platform, the
  audio thread doesn't get enough yield-slices to keep software synthesis
  ahead of DMA consumption — music pitch drops and tempo slows (synthesis
  effectively running at half-rate), SFX gets scratchy from buffer
  underruns. Confirmed empirically at ~21fps wall-clock producing audibly
  broken music on every test cell. Any port doing non-trivial software
  synthesis inside the audio callback needs to know this is an
  architectural ceiling of the cooperative-scheduler model on this
  platform, not something a code fix removes.
- **Upstream's own numbers, and a real mitigation for anyone below
  them.** SDL's own DOS backend documentation (upstream
  [README-dos](https://wiki.libsdl.org/SDL3/README-dos), not a `shared/`
  addition) states the ring buffer holds ~45ms of audio and that "games
  running 22 fps or above require no intervention; below 20 fps, adding
  `SDL_Delay(0)` mid-loop helps maintain buffer levels" — matching
  `shared/patches/sdl3-dos/0023`'s own sizing comment ("4 chunks is
  ~45ms at 44100Hz, enough headroom for 22fps frame times") and
  consistent with the ~21-25fps empirical floor above. **Any port whose
  target frame rate sits below ~20-22fps by design, not as a bug, should
  add an `SDL_Delay(0)` call inside its main loop as a matter of course**
  — this costs nothing on a fast machine (it's a yield, not a sleep) and
  is upstream's own documented answer to exactly this class of port.
- **A port whose frame limiter never calls `SDL_Delay` anywhere gets
  total, permanent silence — not degraded audio, not underruns, zero
  `PlayDevice`/`WaitDevice` activity ever, regardless of what any audio
  callback computes.** This is a stronger and more absolute trap than the
  buffer-headroom guidance above: it's not about *how much* yield time the
  audio pump gets, it's that the pump this backend's DOS audio devices use
  is scheduled cooperatively at `SDL_Delay` call sites specifically (see
  `SDL_dosaudio_sb.c`'s own `DOS_Yield` commentary) — a frame-pacing sleep
  that bypasses `SDL_Delay` (e.g. an engine's own portable sleep
  abstraction backed by a bare `usleep()` on its DOS target, as opposed to
  routing through SDL) never reaches that scheduling point at all, so the
  pump never runs, ever. Confirmed on dossage/Passage: minorGems'
  `Thread::staticSleep()` on its DOS backend
  (`ThreadDOS.cpp`) called plain `usleep()`, never touching SDL, so
  `SDL_HINT_DOS_SILENCE_HISTO=1` showed zero `PlayDevice` emits across
  ~1200 expected calls over 28 real seconds despite `OpenDevice`
  succeeding cleanly — independent of two separate rewrites of the
  callback's own audio-generation logic, since neither rewrite was ever
  the problem. Fixed entirely in that port's own engine code (one call
  site, `Thread::staticSleep` → `SDL_Delay`); no `shared/` change was
  needed or appropriate, since routing a port's own sleep/frame-pacing
  primitive through SDL is squarely that port's responsibility, not
  something the shared backend can detect or fix from underneath an
  engine's own threading abstraction. **Any port whose engine has its own
  sleep/thread/frame-pacing primitive (a portability shim, a threading
  library, a ported engine's own `Sleep()`/`usleep()` wrapper) must audit
  that it ultimately calls `SDL_Delay` on this backend — not just a
  platform-native sleep — or audio can be silently, totally dead for the
  entire session with no error, warning, or partial-failure signal
  anywhere.** doskutsu never hit this because NXEngine-evo's own main loop
  already called `SDL_Delay` directly; the absence of a prior incident
  here reflects that its engine happened to route through SDL already, not
  that this hazard doesn't apply to it.
- **`SDL_Delay` yielding alone is necessary but not sufficient — a
  correctly-yielding port can still underrun on real hardware only,
  because DOSBox-X doesn't faithfully reproduce a single render-gap
  outlasting ring depth.** This backend exports `SDL_DOSAudioPump()`
  (`<SDL3/SDL_dosaudio_pump.h>`, `shared/patches/sdl3-dos/0067`+) for
  exactly this: an engine-callable, explicit mid-gap ring-service call —
  caller pattern `while (SDL_DOSAudioPump()) { }` — meant to be called at
  per-frame strategic points in the main loop, not relied on as a
  side-effect of yielding. It exists because doskutsu itself hit a
  real-hardware-only underrun *with correct `SDL_Delay` yield behavior
  already in place* — a single unusually-long render gap can outlast ring
  depth regardless of the average frame rate, a failure mode DOSBox-X's
  timing doesn't reproduce, so it won't show up in emulator-only testing.
  Confirmed as a real, additional fix on dossage/Passage: after the
  `SDL_Delay` fix above, DOSBox-X's own `PlayDevice` activity was still a
  mixed silent/noisy ratio; adding a per-frame `SDL_DOSAudioPump()` call
  to the main loop (dossage patch 0007) took it to fully non-silent
  (100/100 noisy in the observed window). **Any port doing non-trivial
  audio on this backend should call `SDL_DOSAudioPump()` from its main
  loop, not just call `SDL_Delay` somewhere and assume cooperative
  scheduling alone covers ring health** — `SDL_Delay` gets the pump
  *scheduled at all* (see the trap above); `SDL_DOSAudioPump()` is the
  belt-and-suspenders call that keeps it fed across gaps a DOSBox-X test
  pass won't catch. **Real-hardware confirmed on dossage/Passage.**
  Follow-up finding, though: an unconditional `while (SDL_DOSAudioPump())
  { }` called every frame at a sub-floor frame rate (Passage targets
  15fps, below the ~21-25fps floor two bullets up) hit the defense-in-depth
  32-iterate cap on ~99.5% of calls (424/426 in one measured run) — a
  near-max catch-up burst every single frame is real CPU cost, not just a
  diagnostic curiosity, on hardware already tight on frame budget. Fixed
  by pumping to a modest **target ring depth** each frame instead of
  looping until the ring is full or the cap trips — dossage's fix dropped
  the cap-trip rate to ~3% (24 total) with audio still confirmed audible
  (`vcctrl_audio_verdict` AUDIO_PRESENT throughout, same dB range) and no
  regression, frame rate steady. **Prefer a target-depth top-up over an
  unconditional run-to-cap loop** when calling `SDL_DOSAudioPump()` every
  frame, especially for a port targeting a frame rate at or below the
  cooperative-scheduler audio floor documented above.
- **The silence-detect throttle (`SDL_HINT_DOS_SILENCE_DETECT`,
  `shared/patches/sdl3-dos/0054` + `0056`) has a real fps cost whose shape
  depends on the workload, and whether its killswitch is safe depends on
  the port.** Default-ON, `PlayDevice` scans each mixed chunk for
  all-silence and, on silence, skips the ring write and sleeps
  `SDL_Delay(10)` as a throttle. Two ports have now measured it costing
  two different things. doskutsu's wave-35 A/B attributed a continuous
  +3.34 ms per flip to the scan on an always-busy mix (recorded in
  `0056`'s own commit message). dossage/Passage on a 486DX2-50 paid
  nothing steady-state, but during the quiet passages of its looping
  music track, where nothing gets written to the ring, every cooperative
  pacer yield paid the 10 ms sleep -- three ~20 s windows per life,
  aligned to the song's loop points, enough to fail a 14.90 fps KPI line
  by 0.08-0.18 fps until `SDL_HINT_DOS_SILENCE_DETECT=0` was set before
  `SDL_Init` (`docs/benchmarks/mach64-215ct-486dx2-50-round2-2026-09-04.md`
  and `round3` there, 2026-09-04). Treat the hint's cost as
  workload-shaped -- a small continuous tax on busy audio, a periodic
  larger one on audio with real silence in it -- and A/B it per port
  rather than quoting either number. **The killswitch is only safe for a
  port that does not depend on the silence-detect path for SFX
  correctness:** `0056`'s own message warns that under the OPL3/AdLib
  music backend the shared SfxSynth IRQ-mix never propagates without it,
  so doskutsu's SFX go silent with `=0`; Passage's audio is a plain
  sample stream with no SfxSynth, which is why `=0` was a clean fix
  there. Check which of those your port is before copying the fix.
- **A worse variant of the above can hang indefinitely, and it's
  CPU-tier-specific.** If the audio thread is in-band/non-silent and never
  yields back while the main thread parks at a per-flip yield, the ring
  can starve *without* ever hitting ring-full — so a silence-throttle
  safety net that only fires on ring-full never fires, and the port hangs
  forever. Confirmed to reproduce on a 486DX2-66 but not a faster Pentium
  OverDrive on the identical board — pure timing-window sensitivity.
  **Test any cooperative-scheduler audio path on the slowest target CPU,
  not just the fastest/dev machine** — a hang that never reproduces on a
  fast box can still ship broken to slow ones.
- **A PIT-driven audio pump can silently corrupt SDL's own clock.** DJGPP's
  `uclock()` — the only timebase behind `SDL_GetTicks`/`SDL_Delay`/
  `SDL_GetPerformanceCounter` on this backend — reads PIT channel 0. A
  music pump that reprograms PIT ch0 for its own timing (to drive playback
  without relying on a DMA-IRQ callback) can freeze SDL's clock for up to
  ~55ms at a time, then jump — corrupting any performance measurement
  taken while the pump is active. This produced a false "GUS/AdLib costs
  42% fps" finding that was purely a broken ruler; the real numbers,
  recovered from flip-counts against wall-clock `time()` instead, showed
  no such gap. See `shared/patches/sdl3-dos/0122` for the pump-aware SDL
  timebase mitigation already in the shared backend. **If a port builds
  its own PIT-driven pump: either use a channel DJGPP's timebase doesn't
  read, or explicitly document that `SDL_GetTicks`-family calls are
  untrustworthy while it's active and measure via frame/flip counts
  instead** — this is the same "your measurement instrument can lie"
  class of hazard `dos-hardware-validation` warns about for rig
  configuration; it applies to in-process timing too.
- **DJGPP format-string argument-count mismatches are a hard terminate,
  not a silent misprint.** A logging call with 5 `{}` placeholders and 4
  arguments threw an uncaught `fmt::format_error` -> `std::terminate` ->
  instant quit-to-DOS, at exactly the moment a GUS SFX-upload loop
  completed — this looked exactly like a hardware/DRAM-exhaustion crash
  for multiple diagnostic iterations before someone actually counted the
  placeholders. Audit format-string argument counts specifically on
  DJGPP; a glibc/modern-platform build of the same bug would have just
  misprinted, not crashed, so this class of bug can survive host-side
  testing and only surface as a "hardware crash" on the DOS target.
- **No graceful silent-degrade by default.** With no usable sound device,
  `SDL_INIT_AUDIO` failing is fatal by default — the process exits clean
  to DOS rather than continuing without sound. An "audio device absent,
  run silent" path needs an explicit opt-out a port wires itself; it
  doesn't fall out of SDL failing gracefully on its own.

### GUS / PicoGUS

- **PicoGUS's "USB joystick support enabled" firmware feature can fool
  joystick-detection code, not just audio.** See `docs/input.md`'s
  gameport joystick hazards section -- this is a firmware-level quirk of
  the card, filed under audio here only because that's where PicoGUS
  facts are tracked, not because it's audio-specific.
- **GF1 DRAM is hard-capped at 1 MB** on both real GUS hardware and
  PicoGUS (a 20-bit address space, not a driver limit). A port doing both
  streamed SFX and GM-instrument wavetable music needs an explicit budget
  split between an SFX sample bank and per-song instrument residency —
  `shared/patches/sdl3-dos/0114` partitions the voice pool this way
  (`SDL_HINT_DOS_GUS_SFX_VOICES`, default 4, reserved as the *top* slice so
  an SFX burst can never claim a voice the music mixer already owns,
  clamped to <=1/4 of total voices at device open). This is a real,
  audible trade-off (richer music vs. a few late SFX getting silently
  budget-skipped), not something to tune away.
- **PicoGUS firmware has an exactly-28-voice special case.** Its
  `SCALE_22K_TO_44K` behavior (default-on) forces 28-voice GUS output to
  actual 44.1kHz even when a driver's own rate math assumed 22050Hz at
  that voice count — so 28 voices specifically plays at the wrong internal
  rate/pitch, while 27 or 29 voices work fine. Undiscoverable without
  reading the PicoGUS firmware source directly; if a port's GUS
  voice-count/rate math is naive, avoid the literal number 28 as a
  default.
- **GF1/PicoGUS diagnostic port reads wedge the card.** Any GF1
  register peek/status/readback issued purely as a diagnostic — even from
  main-thread context with no voice active — reliably hangs a PicoGUS,
  confirmed across several independent attempts (a looping test-tone
  during a poke, a DPMI-IRQ-context readback, a main-context readback,
  even heavy log-only instrumentation during song-load). **Never add a
  GF1 port READ to a PicoGUS diagnostic.** Validate the command stream in
  DOSBox-X instead — it's software-determined, identical to real hardware
  for that purpose, and the emulator can't wedge.
- **Large GM patch sets are a trap on a 1MB card.** EAWPATS (built for
  RAM-rich softsynths) has individual patches over 1MB each — a song that
  plays 5-8 instruments under a compact set (Pro Patches Lite) dropped to
  2-3 under EAWPATS, with the rest silently failing DRAM upload. Prefer a
  compact/complete GM patch set for real GUS hardware over the
  biggest-sounding one.
- **MIDI content with out-of-range drum notes silently vanishes** if a
  drum-name/patch lookup table only spans the standard GM percussion range
  (27-87) and the source MIDI uses non-standard channel-10 notes outside
  it — lookup returns null and the note is silently dropped with no log
  and no fallback. Clamp out-of-range drum notes to the nearest defined GM
  percussion instead of dropping them.

### SoundBlaster / SB16

- **DSP-version-based 16-bit-DMA commitment breaks SB-compatible-but-not-
  really cards.** Committing to 16-bit DMA (and hard-erroring on a missing
  high-DMA `H` `BLASTER` field) whenever the reported DSP version is >=4
  breaks PicoGUS-in-SB-mode, which reports a DSP v4-class version while
  only actually supporting 8-bit DMA — see
  `shared/patches/sdl3-dos/0106` for the runtime fallback (force 8-bit
  rather than hard-failing `OpenDevice` on a missing `H`).
- **`BLASTER` env values must match the card's physical jumpers, not
  firmware config.** A firmware config utility tells the card's own
  firmware what to expect, but does not move physical IRQ/DMA lines. The
  mismatch symptom is nasty: DSP detects fine, `OpenDevice` succeeds, but
  zero IRQs ever fire — looks exactly like a driver bug, is actually a
  config/hardware mismatch. `BLASTER` I/D/H values are load-bearing
  hardware facts, not preferences to tune.

### WaveBlaster / MIDI daughterboard

- **WB's MIDI clock rides the SB IRQ.** If that IRQ never fires (see the
  jumper-mismatch trap above), the MIDI scheduler tick never advances —
  but init/transport writes still leak out and can blink the
  daughterboard's MIDI-activity LED, which looks like partial success and
  can send debugging down the wrong layer.
- **WB loudness can't be fixed by GM Master Volume / CC7 boosting if the
  source MIDI already sits near the CC7 ceiling** — additional up-scaling
  just hits the same 127 ceiling and does nothing audible. Check the
  actual MIDI CC7 content before assuming a mixer/gain bug explains a
  quiet wavetable board.
- **The real loudness lever on a real card is the analog Line-In/CD mixer
  input**, not any digital FM/music volume control — WB audio enters
  through the analog mixer path on real SB16 hardware, invisible to a
  purely digital gain stage.

### AdLib / OPL2 vs. OPL3

- **AdLib is OPL2, not "OPL3 minus the second bank."** Writing
  OPL3-bank-2 registers on real OPL2 silently does nothing; correct AdLib
  output needs a genuinely separate OPL2 code path, not a conditional
  inside the OPL3 path.
- **A pure AdLib card has no DSP/DMA at all — PCM SFX architecturally
  cannot play through it.** SFX on an AdLib-only setup needs an entirely
  different device (doskutsu uses PC-speaker square-wave beeps via PIT
  channel 2, gated off while the OPL PIT pump is active to avoid a channel
  conflict).
- **Drum/percussion selection can be indexed by track slot, not by the
  instrument program field**, depending on the source format — scanning
  the wrong field gives a plausible-looking but wrong picture of which
  drums are actually in use.
- **"Missing music" isn't always a playback bug — check the actual note
  timeline first.** One track opened with ~7.7s of channel-10 percussion
  before the first melodic note, in a scene that only lasted 6-8s total —
  every MIDI-rendered backend legitimately spent the whole scene on drums,
  and a shared single noise-burst envelope for every drum slot made that
  sound like near-silence. The fix was per-slot drum envelopes, needed in
  both the OPL3 and OPL2 code paths separately since AdLib has its own
  distinct drum-rendering logic.
