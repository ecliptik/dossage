# Architecture

The stack every port targets, top to bottom:

```
game data
      |
game engine (per-port, e.g. NXEngine-evo for doskutsu)
      |
SDL3 / SDL3_mixer / SDL3_image
      |
SDL3 DOS backend  <- shared/patches/sdl3-dos/
      |
DJGPP + CWSDPMI
      |
MS-DOS 6.22
```

## The cooperative-scheduler model

There is no preemptive multitasking underneath any of this -- DOS has no
threads in the modern sense, and the SDL3 DOS backend's audio, timing, and
I/O all interleave cooperatively: a subsystem callback that doesn't yield
back promptly can monopolize the scheduler and starve everything else
sharing it, not just its own subsystem. Confirmed directly in upstream's
own backend documentation
([README-dos](https://wiki.libsdl.org/SDL3/README-dos)): the mechanism is
a `setjmp`/`longjmp` mini-scheduler that switches only at explicit yield
points (`SDL_Delay`, event pump calls) and never preempts mid-instruction
-- `SDL_Delay(0)` is the documented way to yield without actually
sleeping, worth knowing as a tool independent of the starvation hazard
below. This has a real, load-bearing
consequence for validation: **whether a cooperative-scheduler starvation
bug actually manifests is gated by CPU speed, not by which physical
machine it is.** A callback that reliably yields in time on a fast CPU can
fail to on a slow one running the identical code, on the identical
motherboard/chipset, with only the CPU swapped -- see `audio.md`'s
architecture-wide hazards for the concrete confirmed case (a starvation
bug that reproduced 100% on a 486DX2-66 and never on a faster Pentium
OverDrive on the same board). **Validate any cooperative-scheduler-
sensitive code on the slowest target tier a port claims to support, not
just the fastest/reference machine** -- a fast box can fail to exhibit a
real starvation bug at all, which reads as "fixed" rather than
"untested at the tier where it matters."

`shared/` in this repo owns everything from the SDL3 DOS backend down, plus
the build/test/QA tooling that operates at that layer. A port repo owns
everything from the game engine up, plus the DOS-specific patches that are
genuinely specific to that engine (renderer flip paths, format decoders,
scripting VMs — not video-mode or audio-hardware plumbing).

## What we own vs. what we inherit

| Layer | Source | Patched here? |
|---|---|---|
| Game data | port repo, user-supplied, never committed | no |
| Game engine | port repo, vendored + pinned SHA | port repo's own `patches/<engine>/` |
| SDL3 / SDL3_mixer / SDL3_image | upstream `libsdl-org`, pinned SHA | `shared/patches/sdl3-dos/`, `shared/patches/sdl3-mixer/` |
| DJGPP / CWSDPMI | toolchain, not vendored | not patched |
| MS-DOS | target OS | not patched |

## Seams that matter

Each seam is a place ports have historically hit friction — see the
per-topic docs for detail:

- **Video** (`video.md`): resolution/color-depth assumptions, scaling,
  framebuffer presentation path.
- **Audio** (`audio.md`): PCM vs. MIDI, hardware backend selection, decode
  cost vs. render cost trade-offs.
- **Input** (`input.md`): keyboard/mouse/gameport joystick, no modern
  controller abstraction needed.
- **Filesystem** (`filesystem.md`): 8.3 names, drive letters, no `$HOME`/
  `%APPDATA%`/XDG.
- **DOS scripting** (`dos-scripting.md`): `COMMAND.COM` is not `cmd.exe`
  -- no escape character, redirection parsed inside comments, a 256-byte
  environment block, and other landmines for any port's install/launcher/
  test BATs.
- **Timing** (`timing.md`): decoupling simulation rate from render rate.
- **Optimization** (`optimization.md`): what's actually expensive on
  486/Pentium-class hardware, and in what order to chase it.
- **Testing** (`testing.md`): DOSBox-X/86Box automation vs. real hardware.

Do not design a new port's architecture from scratch — start from this
stack and this seam list, and only diverge where the specific game's engine
genuinely requires it.
