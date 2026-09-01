# Patch conventions

`shared/patches/` and every port repo's own `patches/<engine>/` follow the
same convention, carried forward from doskutsu.

All patches described here are local-only: we never send anything back to
the projects we patch, no PRs/issues/bug-reports — see `CLAUDE.md`'s
"Never contribute upstream" section.

## Layout

```
patches/<vendor>/NNNN-short-description.patch
```

- `<vendor>` is the directory name under `vendor/` (or, in this repo,
  the upstream project the shared patch set targets — currently `sdl3-dos`
  for `libsdl-org/SDL`, `sdl3-mixer` for `libsdl-org/SDL_mixer`).
- `NNNN` is a zero-padded, monotonically increasing four-digit slot,
  produced by `git format-patch` against the pinned upstream commit.
  Reserve slots rather than renumbering existing patches when inserting
  work — insertions get the next free number, not a renumber of history.
- Patches are generated with `git format-patch`, one concern per patch.
  Don't bundle an unrelated fix into a patch whose subject describes
  something else.

## Commit / patch subject

Prefix the subject with a bracketed tag identifying what layer the patch
touches, e.g. `[SDL3-DOS]` for `shared/patches/sdl3-dos/`. Explain **why**,
not just what — a patch whose message says "fix palette bug" is far less
useful later than one that says "S3 ViRGE reports 6-bit DAC width but
accepts 8-bit palette writes; without this the top two bits of every
channel are lost on that chipset."

## Vendor pinning

`vendor/sources.manifest` (doskutsu's format, reused as-is):

```
<name>  <url>  <ref>  <sha>
```

Snapshots pinned by commit SHA, not git submodules — `scripts/fetch-sources.sh`
clones and checks out the pinned SHA, `scripts/apply-patches.sh` then
applies the numbered series on top, `scripts/verify-patches-applied.sh`
sanity-checks patch-file count against applied-commit count. All three live
in `shared/scripts/` and are fully generic — a port repo's own
`vendor/sources.manifest` just adds its engine's entry alongside the SDL3
stack entries.

## Neutral naming inside `shared/`

Because `shared/patches/sdl3-dos/` is consumed by every port, any SDL hint
or log-tag name it introduces must be project-agnostic. Use the
`SDL_HINT_DOS_*` prefix (not a per-port name like the historical
`SDL_HINT_DOSKUTSU_*`) for any new hint, and a similarly neutral log-file
naming scheme (e.g. driven by an env var the port sets, rather than a
hardcoded project name) for any new diagnostic logging. Patches carried
over from doskutsu that introduced `SDL_HINT_DOSKUTSU_*`-style names have
been renamed to this scheme (see `shared/patches/sdl3-dos/README.md`'s
"Naming debt (resolved)" section) -- don't reintroduce a per-port name for
anything new.

## When a port discovers a shared/-relevant fix or lesson

A port's own `patches/SDL` and `patches/SDL_mixer` directories are
symlinks into `.sdl-dos-ports/shared/patches/sdl3-dos`/`sdl3-mixer` (set
up by `scripts/new-port.sh`) — a port session editing a file under
`patches/SDL/` is editing files inside the hub's own subtree content,
merged directly into the port repo's own git history (or, for a port
scaffolded before 2026-08-31, inside a submodule that's a full clone with
the same remote as this repo itself). A subtree-based change can be
pushed back to the hub with `git subtree push --prefix=.sdl-dos-ports
<hub-remote> <branch>`; a submodule-based one can be committed and pushed
directly from inside the submodule checkout. Don't do either as a first
move, for the same reason concurrent, uncoordinated writes to any shared
resource are
risky: `shared/`'s own rules (nothing game-specific, neutral naming,
build + validate under DOSBox-X before considering it done, fix forward
with a new patch number rather than editing patch history for anything
behavioral) need to actually be checked against, and a port session
mid-investigation is optimizing for "unblock my own port," not for "is
this correct for every future port."

**The pattern that works** (developed across doskutsu/dossage's real
cross-port collaboration, not yet exercised as a fully-autonomous
port-pushes-directly flow): a port session that finds something
shared/-relevant — a real bug in `shared/`'s own code, a hazard worth
documenting for every port, a piece of methodology worth reusing —
reports it to whichever session is coordinating the hub (with enough
detail to verify, not just a conclusion: file/patch citations, the actual
evidence, what's confirmed vs. inferred). The hub-side session verifies
it against the actual current state of `shared/` (never take a summary on
faith — this hub's own history already includes a case where an
unverified secondhand relay would have shipped a subtly wrong hazard
writeup), authors the fix or the doc addition following this file's own
conventions, runs the same validation any `shared/` change needs (a full
patch-series replay against the pinned upstream SHA from a clean reset,
not just a diff read), and commits it here. Every consuming port then
bumps its `.sdl-dos-ports` pin and picks it up.

This applies to genuine fixes (a real bug in SDL3-DOS backend code, like
patch `0127`'s hardcoded-path fix) and to documentation/methodology (a
real-hardware hazard worth every port knowing, like the video/audio
hazard catalogs in `docs/video.md`/`docs/audio.md`) equally — "feedback"
flowing back is as much a first-class case as a patch is, not an
afterthought. A port's own engine-specific patches (`patches/<engine>/`)
essentially never belong in `shared/` by `CLAUDE.md`'s own "nothing
game-specific" rule — the cases above are specifically about a port
discovering something true about the *shared* layer or the *platform*
while working on its own engine, not about promoting engine code itself.

## Two gotchas beyond the basics above

- **`LC_ALL=C` when enumerating or sorting patch files.** Locale-aware
  collation (the shell's default) treats `-` as punctuation promoted next
  to alphabetics, so e.g. `0014a-foo.patch` can sort *before*
  `0014-foo.patch` even though ASCII byte order says the reverse --
  `shared/scripts/apply-patches.sh` exports `LC_ALL=C` specifically to
  avoid this, and any other tooling that walks a `patches/<name>/`
  directory and cares about apply order needs the same export. This is
  the same shape of bug as the `find -L` symlink-enumeration fix elsewhere
  in `shared/scripts/` -- a POSIX tool's default behavior silently isn't
  what the enumeration code assumed, and it only surfaces once something
  (a symlinked directory, a locale that isn't `C`) triggers the
  divergence.
- **The patch-cascade base trap.** Authoring a new slot-`N` patch on a
  vendor workspace that already has patches numbered `N+1` and higher
  applied on top bakes the new patch's hunks in against the *wrong* base
  -- `git format-patch` captures whatever the workspace's current state
  is, not the state right after slot `N-1`. Reset the workspace to the
  state immediately after the last patch before the new slot (or apply
  patches strictly in order and insert at the tip, not in the middle)
  before generating the new patch, or the resulting file will fail to
  apply cleanly (or worse, apply with silently wrong context) for the
  next person who runs the series from scratch.
- **Uncommitted vendor-tree work is one `apply-patches.sh` run away from
  gone.** `apply-patches.sh` hard-resets a vendor tree to its pinned SHA
  before applying the patch series -- routine, and destructive to
  anything sitting uncommitted there, including hours of real
  investigation work that hasn't been turned into a patch yet. The
  destructive run is often invoked for a completely unrelated reason (a
  different vendor's patch, or someone else verifying a `shared/` patch
  applies cleanly), so it can destroy work nobody intended to touch.
  `apply-patches.sh` refuses to reset a dirty vendor tree by default
  (`APPLY_PATCHES_FORCE=1` to override) as a safety net -- but the actual
  fix is discipline: commit (even to a scratch branch) or turn working-tree
  edits into a numbered patch at the end of each investigation slice,
  don't let real work live uncommitted in a vendor tree for hours.

## A patch series' stated rationale is not evidence

When reading another engine's patch history for precedent (e.g. citing
doskutsu's own history to justify a design choice in a new port), read
the actual gating/guard code the patches touch, not just the commit
titles or the rationale prose in later patches' comments. A real case on
this hub: a patch's own comment cited an earlier patch's stated
motivation as if it were settled fact, when a later patch in the same
series had already directly tested that motivation and found it false (a
controlled A/B showed the fix bought no correctness benefit at all, only
cost). The series' own history disproved its opening premise several
patches later — and the disproof was recorded, but easy to miss if you
stop reading at the first patch's comment. Trust what the code's actual
conditionals do (what's gated behind what flag, under what real
condition) over what any single patch's commit message claims it
accomplishes.

## Verify provenance before landing a peer-authored patch

A patch handed over through a chat/message channel (another session
pasting a diff rather than handing over a real file) can lose fidelity in
transit — trailing whitespace on blank context lines is a documented real
case, silently stripped by common text-transport paths, which can corrupt
a diff without any visible sign in the pasted text. Before landing a
peer-authored patch in `shared/patches/`:

- Prefer a real `git format-patch` output (with genuine commit metadata)
  over a hand-typed or reconstructed diff — ask the author to generate it
  from their own working checkout rather than reconstruct it yourself
  from a chat message, if you don't have the toolchain to verify it
  applies cleanly.
- If you must reconstruct one from pasted text, say so explicitly and
  flag the reconstruction as unverified until the author (or a fresh
  `apply-patches.sh`/`git am` run against a real checkout) confirms it
  applies byte-for-byte.
- Review the diff's actual content against this hub's own conventions
  (binary `fopen` mode, ASCII-only, neutral naming) before landing it —
  don't assume a peer session's own review caught everything; a real
  instance of this caught a text-mode-`fopen` bug that every prior patch
  in the same series had avoided.

## A temporary diagnostic patch needs its removal planned at landing time

A diagnostic patch landed in `shared/` — gated or not, cheap or not —
becomes a permanent, silent tax on every port that pins past it the
moment its investigation concludes and nobody schedules its removal. A
real instance: a temporary `DOS_Yield()` cost diagnostic, landed with its
own commit message stating "temporary, to be reverted once the
investigation concludes," measured a genuine per-frame cost
(~0.70ms/frame). The investigation concluded — the diagnostic had done
its job — but the removal patch wasn't authored in the same pass. Every
port pinning past that commit inherits the cost with nobody having
decided to pay it, and a rebuild that suddenly runs measurably slower has
no obvious reason to suspect the patch series rather than the change
someone just made.

The convention: when landing a temporary diagnostic, either land its
removal in the same session once the measurement is in hand (this hub's
own patch `0028` is precedent — land, measure, strip, all as part of one
investigation's patch sequence), or explicitly note in the landing
commit's message that a removal patch is still owed and by what
condition ("once X is measured" / "once Y concludes") — never leave it as
an implicit, unscheduled follow-up. A reverted diagnostic's own removal
patch should confirm (not just claim) that the resulting binary is
byte-identical to what the pin produced before the diagnostic ever
landed.

## Negative results are worth keeping

`shared/patches/sdl2-compat-notes/` carries forward doskutsu's write-up of
why a static `sdl2-compat`-on-DJGPP approach didn't work — keep documenting
dead ends like this so the next port doesn't re-spend the time proving the
same approach fails.
