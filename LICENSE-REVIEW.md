# License review: dossage (Passage)

## Engine source

License: **Public domain.**

Redistribution terms (source): unrestricted. No LICENSE/COPYING file ships
in the `jasonrohrer/Passage` tree itself; the verification source is the
author's own project page, `https://hcsoftware.sourceforge.net/passage/`,
which states explicitly: **"Placed in the public domain."** Read directly
(not recalled/assumed) on 2026-08-26 against the pinned revision
(`2f713f261dc907f6feda106ebbee0464eeab791d`).

Redistribution terms (compiled binary): unrestricted, same basis.

Notes: Passage's Makefiles (`Makefile.GnuLinux`, `.MacOSX`, `.MinGW`)
confirm raw SDL 1.2 linkage only (`-lSDL -lpthread` on Linux) -- no
SDL_mixer/SDL_image dependency, consistent with the "tiny engine surface"
finding in `PLAN.md`.

## Build dependency: minorGems

License: **Public domain.**

Redistribution terms: unrestricted. Verified directly against an in-repo
file at `jasonrohrer/minorGems`'s root, `no_copyright.txt`, dated March
2018:

> "This work is not copyrighted. I place it into the public domain. Do
> whatever you want with it, absolutely no restrictions, and no permission
> necessary. -- Jason Rohrer, Davis, California, March 2018"

This is a stronger verification than Passage's own (an actual file in the
pinned tree, not a webpage), and it is the same author granting the same
terms for both projects.

Notes: minorGems is a large, actively-developed multi-project library
(network/p2p code, `sound/portaudio` and `network/upnp/miniupnpc`
third-party vendor trees with their own separate licenses, crypto, AI,
etc.). **Only a small subset is used by Passage** -- file/path handling,
string utilities, settings persistence, time, thread (sleep only), SHA1,
TGA image decoding, and a simple-vector container. Only that subset should
be vendored/built for this port; the third-party-licensed subtrees
(portaudio, miniupnpc) are irrelevant here and must not be pulled in.

## DOS-specific patches (this port's own `patches/passage/`, `patches/minorgems/`)

License: public domain (inherits the engine's license -- a derivative of a
public-domain work carries no restriction).

Notes: n/a.

## SDL3 / SDL3_mixer + `shared/patches/`

License: zlib (see sdl-dos-ports' `THIRD-PARTY.md`). SDL3_image is **not**
needed for this port -- Passage's `.tga` assets are decoded by minorGems'
own `TGAImageConverter`, not SDL_image.

Notes: n/a.

## Game assets

Graphics license: public domain (same Passage repo, same author statement
as engine source above -- `gameSource/*.png`/`.tga`).

Music license: public domain (`gameSource/music/music.tga` -- a
data-encoded score for Passage's own software synthesizer, not an audio
file; same statement covers it).

Font license: n/a -- no font library; score digits render from a bitmap
(`numerals.png`/`.tga`), same license as the other graphics.

Redistribution rights: unrestricted.

Can this port ship any game data at all? **Yes.** Unlike the default
"engine only, user supplies their own data" model this hub uses for
freeware-but-not-public-domain games (doskutsu/Cave Story), Passage's
engine and assets are both public domain from the same author, so this
repo ships the complete game, data included.

## Conclusion

DOSSAGE can distribute the complete game -- engine, DOS-port patches, and
all graphics/music assets -- without restriction, because Passage and its
minorGems dependency are both public domain, verified directly against the
author's own statements (project page for Passage, in-repo
`no_copyright.txt` for minorGems). The DOS-port-specific code (build
system, patches, docs original to this repo) is separately MIT-licensed
(`LICENSE`) for clarity, though nothing here legally requires that choice.
Bundled releases must still include CWSDPMI's own redistribution terms
(`CWSDPMI.DOC`, freeware/redistributable) alongside the binary, per the
DPMI host's own license -- see `.sdl-dos-ports/docs/licensing.md`'s
downstream-redistribution checklist pattern.
