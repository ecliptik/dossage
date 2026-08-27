# DOSSAGE

DOSSAGE is a port of Jason Rohrer's [Passage](https://hcsoftware.sourceforge.net/passage/) (2007) to MS-DOS on retro 486/Pentium-class hardware. It plays Rohrer's five-minute memento-mori game on real 1990s-era PCs via [SDL3](https://www.libsdl.org/)'s [DOS backend](https://github.com/libsdl-org/SDL/pull/15377), [DJGPP](https://www.delorie.com/djgpp/), and [CWSDPMI](https://en.wikipedia.org/wiki/DOS_Protected_Mode_Interface).

The name is a portmanteau of **DOS** and **Passage**, matching the naming convention of its sibling port [doskutsu](https://forgejo.ecliptik.com/ecliptik/doskutsu) (DOS + Doukutsu Monogatari).

DOSSAGE exists for preservation and the engineering challenge of running Passage on a 1990s MS-DOS PC. It is also intended to become the copy-paste starting skeleton for future ports in the [sdl-dos-ports](https://forgejo.ecliptik.com/ecliptik/sdl-dos-ports) hub, once finished -- Passage's engine surface (raw SDL 1.2, no mixer/image/font libraries) is about as small as a real DOS port gets.

### Screenshots

| | |
|:---:|:---:|
| <img src="docs/screenshots/dossage-title.png" alt="DOSSAGE title screen running in DOSBox-X" width="100%"> | <img src="docs/screenshots/dossage-gameplay.png" alt="Passage gameplay -- maze corridor, player sprite, treasure chests -- running in DOSBox-X" width="100%"> |
| **Title Screen** | **Gameplay** |

<p align="center">captures from DOSBox-X running <code>DOSSAGE.EXE</code></p>

---

## Status

**PLAYABLE.** The SDL 1.2 -> SDL3 migration and DJGPP platform port compiled and linked clean, and boots to the title screen and into gameplay under DOSBox-X with real-MS-DOS-representative filesystem settings (`lfn=false`, all asset/settings filenames renamed 8.3-safe) -- verified 2026-08-26. Audio device init and the synthesis math are confirmed correct (see `PLAN.md`); real audibility is deferred to real hardware, since DOSBox-X in this dev environment can't conclusively demonstrate playback. No real-hardware run yet. See [PLAN.md](./PLAN.md) and this port's entry in the hub's [ports.yaml](https://forgejo.ecliptik.com/ecliptik/sdl-dos-ports/src/branch/main/ports.yaml) for current milestone state, and [STATUS.md](./STATUS.md) for the structured summary.

**Target.** Passage's own source (`gameSource/game.cpp`) locks a `lockedFrameRate` of **15 fps** -- much lower than doskutsu's 50 fps design rate -- so this port's performance bar is hitting that original 15 fps, sustained, on the DOS minimum/recommended target below, rather than chasing the highest frame rate possible.

| CPU | Role |
|---|---|
| 486DX2-50 | minimum and recommended target (per `ports.yaml`) |

Once playable and optimized, this port will be measured across the same CPU / video-card / sound-card matrix doskutsu uses (see its [docs/BENCHMARKS.md](https://forgejo.ecliptik.com/ecliptik/doskutsu/src/branch/main/docs/BENCHMARKS.md) for the shape that matrix takes) on the same physical rig, currently configured as a Pentium OverDrive 83 MHz / 48 MB RAM / PicoGUS / Mach64.

---

## Game Assets

Unlike doskutsu, **DOSSAGE ships its own game data.** Passage's engine *and* its graphics/music assets were placed in the public domain by Jason Rohrer himself -- see [Components and License](#components-and-license) below -- so there is no separate "supply your own copy" step. The `.tga` graphics and the synthesized music data are vendored alongside the engine source, pinned to the same upstream revision.

---

## Requirements

Passage's original engine surface is tiny (no codec libraries, no font rendering, a from-scratch software synthesizer) -- the DOS requirements are correspondingly modest.

**Target**

- CPU: 486DX2-50 or faster
- Video: VESA 1.2+
- Sound: Sound Blaster-compatible (via SDL3-DOS's audio backends), or none -- Passage's own audio is optional
- OS: MS-DOS 6.22 or compatible

Finalized once the port reaches `PLAYABLE` and real-hardware measurement confirms the floor.

---

## Building

Building needs a Linux (or WSL) host with:

- the [DJGPP](https://github.com/andrewwutw/build-djgpp) cross-compiler
- `cmake`, `git`, `make`, `gcc`, `python3`
- `dosbox-x` -- runs the automated build-verification smoke tests

This repo consumes the [sdl-dos-ports](https://forgejo.ecliptik.com/ecliptik/sdl-dos-ports) hub's shared SDL3-DOS platform layer as a git submodule at `.sdl-dos-ports/`:

```bash
git clone https://forgejo.ecliptik.com/ecliptik/dossage.git
cd dossage
git submodule update --init --recursive
./scripts/setup-symlinks.sh          # one-time: link tools/djgpp (if using the ~/emulators hub)
./scripts/fetch-sources.sh           # clone the upstream repos at pinned SHAs
./scripts/fetch-vendor-binaries.sh   # fetch CWSDPMI.EXE
./scripts/apply-patches.sh           # apply DOS-port patches
make sdl3                            # cross-build SDL3 (no SDL3_mixer/SDL3_image -- unneeded)
make game                            # build/dossage.exe
make stage                           # build/stage/ -- DOSSAGE.EXE + CWSDPMI.EXE + assets together
tools/dosbox-launch.sh --fast --stage --exe DOSSAGE.EXE   # smoke-test under DOSBox-X
```

---

## How This Project Is Developed

DOSSAGE is developed agentically with [Claude Code](https://claude.com/code), following the same model as doskutsu:

- **Claude Code authors the patches** across the SDL3 DOS backend and the Passage engine. They land as `patches/<vendor>/NNNN-*.patch` files in this repository.
- **Human developers drive testing and iteration**: real-hardware playthroughs, bug reports, and deciding what to fix next.
- **Workspace-local patches only.** This project does not contribute patches upstream to [libsdl-org/SDL](https://github.com/libsdl-org/SDL), [libsdl-org/SDL_mixer](https://github.com/libsdl-org/SDL_mixer), [jasonrohrer/Passage](https://github.com/jasonrohrer/Passage), or [jasonrohrer/minorGems](https://github.com/jasonrohrer/minorGems).

---

## Components and License

DOSSAGE's own source -- the build system, scripts, and documentation -- is **MIT-licensed** ([LICENSE](./LICENSE)). Unlike doskutsu, the shipped binary carries no copyleft obligation: Passage and its minorGems dependency are both public domain, and SDL3 is zlib. See [LICENSE-REVIEW.md](./LICENSE-REVIEW.md) for the full review.

| Component | Purpose | License | In `DOSSAGE.EXE` |
|---|---|---|---|
| [DOSSAGE port source](./LICENSE) (this repo) | Build system, patches, scripts, docs | MIT | n/a - source, not the binary |
| [Passage](https://github.com/jasonrohrer/Passage) | The game itself, by Jason Rohrer (2007) | [Public domain](https://hcsoftware.sourceforge.net/passage/) | Yes |
| [minorGems](https://github.com/jasonrohrer/minorGems) | Rohrer's own utility library (file/string/time/thread/TGA-decode subset only) | [Public domain](https://github.com/jasonrohrer/minorGems/blob/master/no_copyright.txt) | Yes |
| [SDL3](https://www.libsdl.org/) | Platform layer; its [DOS backend](https://github.com/libsdl-org/SDL/pull/15377) is what makes the port possible, including audio (Passage's own synth drives SDL3's core audio-stream API directly -- no SDL3_mixer, no file-decode codec) | [zlib](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt) | Yes |
| [DJGPP](https://www.delorie.com/djgpp/) libc | 32-bit DOS C runtime, by DJ Delorie | [GPL + runtime exception](https://www.delorie.com/djgpp/v2faq/faq11_2.html) | Yes - the exception permits static linking |
| [CWSDPMI](https://www.delorie.com/pub/djgpp/current/v2misc/) | DPMI host, by Charles W. Sandmann | freeware, redistributable | No - ships alongside as a separate program |

Full attribution detail: [THIRD-PARTY.md](./THIRD-PARTY.md) (added once dependencies are vendored).
