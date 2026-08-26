# Third-Party Components

Complete attribution and license matrix for everything DOSSAGE touches,
vendors, or ships. Kept in sync with `vendor/sources.manifest`.

---

## At-a-glance

| Component | Version / Ref | License | Shipped in dist? | Role |
|---|---|---|---|---|
| [Passage](https://github.com/jasonrohrer/Passage) | `master` @ `2f713f2` | **public domain** | Yes (statically linked + data) | The game itself, by Jason Rohrer (2007) |
| [minorGems](https://github.com/jasonrohrer/minorGems) (subset) | `master` @ `ef42b1c` | **public domain** | Yes (statically linked) | File/path, string, settings, time, thread, sha1, TGA-decode utility subset |
| [SDL3](https://github.com/libsdl-org/SDL) | `main` @ `74a7462` (post-[PR #15377](https://github.com/libsdl-org/SDL/pull/15377)) | zlib | Yes (statically linked) | Platform abstraction + DOS backend |
| [SDL3_mixer](https://github.com/libsdl-org/SDL_mixer) | `release-3.2.x` @ `e69654e` | zlib | Yes (statically linked) | Audio device/stream plumbing (Passage's own synth drives the samples; no file-decode codec is used) |
| [DJGPP libc](https://www.delorie.com/djgpp/) | 2.05+ (via GCC 12.2.0) | **GPL + runtime-library exception** | Yes (statically linked) | C runtime on DOS |
| [CWSDPMI](https://sandmann.dotster.com/cwsdpmi/) | TBD (not yet vendored) | **freeware, redistribution permitted** | Yes (separate .exe, not linked) | DPMI host |
| [DOSBox-X](https://dosbox-x.com/) | system package | GPLv2 | No (dev-only) | Pre-hardware testing emulator |

Note: **SDL3_image is not vendored** -- Passage's `.tga` graphics are
decoded by minorGems' own `TGAImageConverter`, so there is nothing for it
to do here.

---

## License compatibility analysis

### The binary carries no copyleft obligation

Unlike doskutsu (whose `DOSKUTSU.EXE` is GPLv3 because it statically links
GPLv3 NXEngine-evo), `DOSSAGE.EXE` links only public-domain (Passage,
minorGems) and zlib-licensed (SDL3, SDL3_mixer) code, plus DJGPP libc
(GPL with a runtime-library exception that explicitly permits static
linking without imposing GPL on the result). There is no GPL-proper
component in the link line, so nothing forces the combined binary under a
copyleft license.

### MIT source, public-domain engine

- This repo's own source (build system, scripts, docs, patch headers) --
  **MIT**.
- `patches/SDL/*.patch`, `patches/SDL_mixer/*.patch` -- derivatives of
  zlib-licensed upstreams, therefore zlib.
- `patches/passage/*.patch`, `patches/minorgems/*.patch` -- derivatives of
  public-domain upstreams. A derivative of public-domain work carries no
  restriction; these patches are effectively public domain too, and in any
  case impose no obligation on this repo's own MIT license.

No conflicts.

### CWSDPMI is separate

Same posture as doskutsu: CWSDPMI is a DPMI host invoked at runtime, not
linked. Its redistribution terms require bundling `CWSDPMI.DOC` alongside
the binary. Not yet vendored in this repo -- tracked as a Phase 1/build-
system task (see `PLAN.md`), following doskutsu's `fetch-vendor-binaries.sh`
+ `vendor/binaries.manifest` pattern rather than guessing a source/sha now.

### Passage's game data ships with the engine

Because Passage's graphics and music data are public domain from the same
author as the engine, they are vendored and shipped in this repo -- unlike
Cave Story's freeware-but-not-redistributed data in doskutsu. See
`LICENSE-REVIEW.md` for the full reasoning and the exact sources verified.

---

## Full per-component detail

### Passage

- **License:** public domain (see `LICENSE-REVIEW.md` for the verified
  source -- the author's own project page, no in-tree LICENSE file)
- **Source:** https://github.com/jasonrohrer/Passage
- **Pinned ref:** `master` @ `2f713f261dc907f6feda106ebbee0464eeab791d`
- **Role:** The game itself -- engine (`gameSource/*.cpp`) and data
  (`gameSource/*.tga`, `music/music.tga`).
- **Modifications:** DOS-port patches in `patches/passage/*.patch`.

### minorGems (subset)

- **License:** public domain (verified via in-repo `no_copyright.txt`)
- **Source:** https://github.com/jasonrohrer/minorGems
- **Pinned ref:** `master` @ `ef42b1ce511f2d355d5fc898fcce0b0af3a76d62`
  (a starting point -- see `PLAN.md`'s "Open pin risk" note; may walk
  backward if API drift surfaces during the compile phase)
- **Role:** File/path, string, settings, time, thread, sha1, TGA image
  decode, and simple-vector utilities Passage's build depends on. Only
  this subset is vendored/built -- minorGems' other subtrees (portaudio,
  miniupnpc, network/p2p, etc.) are irrelevant here.
- **Modifications:** DOS-port patches (notably a DJGPP `Path`/`Time`
  platform variant, which doesn't exist upstream) in
  `patches/minorgems/*.patch`.

### SDL3

- **License:** zlib
- **Source:** https://github.com/libsdl-org/SDL
- **Pinned ref:** `main` @ `74a746281f2208e07a7680560fcb7ec57565228e`
  (same pin as doskutsu's manifest, matching the shared
  `patches/sdl3-dos/` series this repo consumes via `.sdl-dos-ports`)
- **Role:** Platform abstraction; the DOS backend is what makes the port
  possible.
- **Modifications:** shared `.sdl-dos-ports/shared/patches/sdl3-dos/`
  series, symlinked at `patches/SDL/`.

### SDL3_mixer (release-3.2.x)

- **License:** zlib
- **Source:** https://github.com/libsdl-org/SDL_mixer
- **Pinned ref:** `release-3.2.x` @ `e69654e9eac6d07ec5dfb9d9db022e836d3d1e11`
- **Role:** Audio device/stream plumbing. Passage's own software synth
  drives raw samples directly -- no WAV/OGG/MP3/MIDI file-decode path is
  used from this library.
- **Modifications:** shared `.sdl-dos-ports/shared/patches/sdl3-mixer/`
  series, symlinked at `patches/SDL_mixer/`.

### DJGPP libc

- **License:** GPL with the runtime-library exception
- **Source:** https://www.delorie.com/djgpp/
- **Role:** C runtime for DJGPP-compiled binaries.

### CWSDPMI

- **License:** freeware with specific redistribution terms
- **Source:** https://sandmann.dotster.com/cwsdpmi/
- **Role:** DPMI host. Not yet vendored -- see "CWSDPMI is separate" above.

### DOSBox-X

- **License:** GPLv2
- **Role:** Pre-hardware testing emulator. Dev tool only -- not shipped,
  not linked.

### Sibling / hub projects

- **doskutsu** (reference port, doc style): https://forgejo.ecliptik.com/ecliptik/doskutsu -- MIT
- **sdl-dos-ports** (shared SDL3-DOS platform layer, consumed as a
  submodule): https://forgejo.ecliptik.com/ecliptik/sdl-dos-ports -- MIT

No code from doskutsu is linked into DOSSAGE; it is a structural and
stylistic reference only. `sdl-dos-ports`'s `shared/` layer *is* consumed
directly, via the `.sdl-dos-ports` submodule.

---

## Verification

Before cutting a release, verify the dist archive contains:

- [ ] `DOSSAGE.EXE`
- [ ] `CWSDPMI.EXE`
- [ ] `CWSDPMI.DOC` (CWSDPMI license, required by its terms)
- [ ] `LICENSE.TXT` (this repo's MIT + public-domain note)
- [ ] `THIRD-PARTY.TXT` (CRLF-normalized version of this file)
- [ ] `README.TXT` (user-facing DOS-readable quick-start)

The `dist` Makefile target (once it exists) is the source of truth for
what ends up in the archive. If it diverges from this list, fix the
Makefile and this document together.
