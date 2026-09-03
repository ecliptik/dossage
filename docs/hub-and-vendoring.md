# The hub subtree and per-port vendoring

How this repo relates to
[sdl-dos-ports](https://forgejo.ecliptik.com/ecliptik/sdl-dos-ports), which
files are ours to edit, and which will be silently reverted if we edit them.

None of this is derivable from the code -- a subtree looks exactly like
ordinary source until the day it reverts your work -- which is why it is
written down rather than left to be rediscovered.

## Two mechanisms, not one

The hub reaches this repo through **two independent paths**, and conflating
them is the source of most confusion:

| | Mechanism | Who owns it | Edit here? |
|---|---|---|---|
| `patches/SDL/`, `patches/passage/`, `patches/minorgems/` | **vendored** -- real files, copied once | this repo | **Yes** |
| `.sdl-dos-ports/` (everything else) | **git subtree** | the hub | **No** |

### Vendored: our patch series

Since 2026-09-01 (this repo's `d70cf26`, hub `efe8ba0`) every port owns a
real copy of its patch series. `patches/SDL/` is 125 real files, not a
symlink into the hub. Consequences:

- Author new SDL patches directly in `patches/SDL/<NNNN>-*.patch`, same
  numbering discipline as `patches/passage/`.
- **No sync in either direction.** A fix landed in the hub after that date
  does not reach `patches/SDL/` on its own, and a fix made here does not
  reach the hub. Moving one is a deliberate, reviewed copy -- exactly as
  between any two independent repos, because that is now what they are.
- The `patches/<vendor>-local/` overlay is **retired**. With everything
  vendored per-port there is no shared-vs-local distinction left to draw;
  a temporary diagnostic is just another numbered patch here, and still
  needs its removal planned when it lands.

### Subtree: everything else

`.sdl-dos-ports/` is a git **subtree**, not a submodule (converted
2026-08-31, `e474e8b` + `975e2ca`). The hub's files are therefore ordinary
tracked files in this repo's own history.

That buys a checkout that always works -- a plain `git clone` gets
everything, with no `--recursive` and no `git submodule update --init`.
That matters because a lot resolves through that path: the `Makefile`'s
`include $(HUB_DIR)/shared/build/sdl3-dos.mk`, the four symlinks in
`scripts/`, and the `.claude/skills/` symlinks. Under a submodule, an
uninitialized checkout turns all of those into dangling symlinks and a
missing-include error.

What still comes from the subtree:

- `shared/build/sdl3-dos.mk` -- the entire SDL3 cross-build
- `shared/scripts/*.sh` -- fetch, apply, verify, setup (symlinked into `scripts/`)
- `shared/tools/` -- DOSBox-X automation
- `shared/tests/probes/` -- the probe library
- `shared/agents/`, `shared/skills/`, `docs/` -- charters, skills, porting docs
- `shared/audio/midi_sched.c`, `shared/include/runmanifest.h` -- present, not currently used here

## The trap

**Editing a file under `.sdl-dos-ports/` succeeds, stages cleanly, commits
normally, and never reaches the hub. The next `git subtree pull` silently
reverts it.**

Git warns you at no point. There is no nested `.git`, no `modified
content` line, nothing distinguishing it from editing this repo's own
code. Under the old submodule this was structurally obvious -- you were in
a different repository and simply could not commit from here. The subtree
traded that guardrail away for the reliable checkout.

`make hooks` installs a pre-commit hook that refuses such commits.
`core.hooksPath` is local config and is not carried by a clone, so a fresh
checkout has to run it once.

Merge commits are exempt (a subtree pull lands as a merge). For a
deliberate local-only edit -- debugging the build fragment in place, say --
use `ALLOW_HUB_EDIT=1 git commit ...`, and expect the next pull to revert
it. That is the mechanism working, not a bug.

## Landing a change in `shared/`

    # 1. clone the hub, branch, make the change there
    git clone ssh://git@forgejo.ecliptik.com/ecliptik/sdl-dos-ports.git
    # 2. test it against this repo by pointing at your clone's scripts
    # 3. commit and push to the hub
    # 4. bring it back:
    git subtree pull --prefix=.sdl-dos-ports \
        ssh://git@forgejo.ecliptik.com/ecliptik/sdl-dos-ports.git main --squash

The subtree pin is not machine-readable the way a submodule's was: it
lives only in the squash commit's message (`Squashed '.sdl-dos-ports/'
changes from <a>..<b>`). To check whether you are current, read that
message and compare against `git ls-remote <hub> HEAD`.

## Why this is worth a hook: the stale-build hazard

The per-port-vendoring migration moved the patch series to
`patches/SDL/` but left `shared/build/sdl3-dos.mk` globbing
`$(HUB_DIR)/shared/patches/sdl3-dos/*.patch` for `libSDL3.a`'s
prerequisites. `apply-patches.sh` and `verify-patches-applied.sh` had both
moved to the port's own `patches/<name>/`.

So adding a patch changed what got built without changing what `make`
thought the build depended on. Make short-circuited the recipe, `cmake`
never re-ran, and `libSDL3.a` went silently stale -- while
`verify-patches-applied` passed, because its counts still matched.

Fixed in hub `db81f67`; both variables now root at `$(REPO_ROOT)/patches/`.
The lesson is that a wrong path in the subtree is not a wrong path, it is
a wrong *build*, and it fails green.

## The verification gate

`verify-patches-applied.sh` runs before every build stage and checks three
things per vendor:

1. **Count** -- patch files vs vendor commits since the pin.
2. **Duplicate slots** -- two patches claiming the same `NNNN` prefix.
3. **Content** -- a fingerprint of the series recorded by
   `apply-patches.sh` at apply time (`build/patch-series/<name>.sha`)
   against the series on disk now.

Check 3 exists (hub `efe53ee`) because 1 is blind to an in-place **edit**:
change a patch file without adding or removing one and the counts still
match. That went from theoretical to live with per-port vendoring, since
the series is now an ordinary editable file here -- and since `db81f67`
the build correctly keys on those files, so an edit triggers a rebuild of
the *un-re-applied* vendor tree.

A tree with no fingerprint fails rather than passing quietly. Fix is one
idempotent command:

    make patches

Note it compares the series' bytes, not diffs. Comparing `git patch-id`
against the vendor commits was tried and rejected: patch-id stays
sensitive to how a diff was generated, and this repo has patches written
with zero context lines beside a vendor tree whose `git show` emits three.
Same change, same resulting tree, different patch-id.

## Reproducibility, verified 2026-09-02

- `vendor/SDL` re-applies to tree `b9660a14d0cacda1bd6124e498d8b159fee6cb73`
  from the pin plus the 125 vendored patches -- matching the baseline
  recorded during the vendoring migration.
- The DOS binary build is **deterministic**: two independent builds from a
  full `make game-clean` produced byte-identical `dossage.exe`
  (`sha256 750a5952777c...`).

See `BENCHMARK-PLAN.md` for how that interacts with the campaign's build
pin.
