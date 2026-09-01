---
name: review
description: >
  Reviewing a patch destined for shared/patches/, a port's own
  patches/<engine>/, or a port's own patches/<vendor>-local/ overlay for
  a shared vendor -- against this hub's own landing conventions before
  it's committed: which of those three it actually belongs in (reusable
  fix vs. disposable single-port instrumentation), provenance
  verification, DJGPP hard constraints, neutral naming, numbering/slot
  discipline, and the "read gating code, not commit titles" rule. Use
  this whenever a patch (yours, a peer session's, or a teammate's) is
  about to be committed anywhere in this hub or a port repo, especially
  one authored or handed over by a different session.
---

# Reviewing a patch before landing it

Distinct from the generic `/code-review` command: this checks a patch
against *this hub's own* conventions (`docs/patch-conventions.md`), not
general code quality. Run every step; a patch that's correct C but wrong
by these conventions still shouldn't land.

## 1. Shared series, or a port-local overlay?

Before any other check, for a patch touching a *shared* vendor (SDL,
SDL_mixer): does this belong in `shared/patches/<vendor>/` at all, or in
the port's own `patches/<vendor>-local/`? Ask "would the next
investigation on a *different* port want this?" — never "which file does
it touch" or "is this a diagnostic" (both fail on real boundary cases;
see `docs/patch-conventions.md`'s "Shared series vs. a port-local
overlay" section for why). If you're not sure, default local — promoting
a proven-reusable local patch into the shared series later is cheap;
removing a mistakenly-shared one after other ports have pinned past it
isn't. This decision is prior to everything below; steps 3 and 5 in
particular only fully apply once a patch is destined for the shared
series specifically.

## 2. Verify provenance

Is this a real `git format-patch` output (genuine commit hash/author/date)
or a hand-typed/reconstructed diff? If it arrived through chat, treat it
as unverified until confirmed byte-for-byte against the author's own
checkout — chat transport silently strips trailing whitespace on blank
context lines. See `docs/patch-conventions.md`'s provenance section.

## 3. Check the slot number

Is the `NNNN` the actual next-free slot in the *target* series (`ls |
sort | tail` in `shared/patches/<vendor>/` for a shared-series patch, or
in the port's own `patches/<vendor>-local/` for a local-overlay one --
the two numbering sequences are independent), not a guessed or stale
number? Slots are reserved, never renumbered.

## 4. Check DJGPP hard constraints

Binary `fopen` mode (`"rb"`/`"wb"`/`"ab"`, never text mode) for anything
not text, 32-bit `size_t` assumptions only, ASCII-only (no smart quotes,
em-dashes, other non-ASCII), no SIMD unless `NOSIMD_FLAGS`-gated, no
shared libraries. Grep the diff for `fopen(` and check every mode string
individually — a series with a consistent binary-mode convention can
still have one line that regresses it. Applies to a local-overlay patch
too, same DJGPP target either way.

## 5. Check neutral naming (`shared/` series only)

Any new `SDL_HINT_*` or log-file name must be project-agnostic
(`SDL_HINT_DOS_*`, not a per-port prefix) — see
`docs/patch-conventions.md`'s neutral-naming section. Not applicable to a
port's own `patches/<engine>/`, or to a `patches/<vendor>-local/`
overlay patch (it's port-owned and never seen by another port, so a
port-specific name there is fine, even expected).

## 6. If it cites another engine's patch history as precedent, read the actual gating code

Don't trust a cited patch's commit title or a later patch's summary of an
earlier one's rationale — read what's actually gated behind what
condition. A patch series' own stated rationale is not evidence; a real
case on this hub had a later patch in the same series directly disprove
an earlier patch's premise. See `docs/patch-conventions.md`.

## 7. If it's a temporary diagnostic destined for the shared series, confirm its removal is scheduled

If step 1 correctly sent this to a `patches/<vendor>-local/` overlay
instead, this step doesn't apply -- a local-overlay patch is already
scoped to one port and never taxes another, so there's no "every port
pinning past it" hazard to schedule around. For the rarer case where a
temporary diagnostic genuinely belongs in the shared series (reusable
tooling the next investigation would also want, per step 1's criterion):
it needs its removal either landed alongside it once the
measurement is in hand, or an explicit note in the commit message of what
condition triggers the removal patch. Don't land an open-ended "temporary"
patch with no removal plan — see `docs/patch-conventions.md`.

## 8. Confirm it builds and smokes clean

Per `CLAUDE.md`: a change to `shared/` isn't done until validated under
DOSBox-X. If you don't have the toolchain to do this yourself, say so
explicitly and get the validation from whoever does (see
`shared/agents/README.md`'s team-shape guidance) before landing — never
land an unvalidated shared-layer change silently.

## Generating the patch yourself?

Commit working-tree vendor edits into a numbered patch at the end of
each investigation slice, not at the end of a campaign — `apply-patches.sh`
refuses to reset a dirty vendor tree by default, but that's a safety net,
not a substitute: hours of real work sitting uncommitted in a vendor tree
can still be lost to a routine `apply-patches.sh` run made for a
completely unrelated reason before that check was added, or on a tree
someone deliberately forces past it.

## What this skill does not replace

Real hardware validation of the *result* the patch produces (see
`dos-hardware-validation`) — this skill reviews the patch as a patch, not
whether the underlying fix/feature actually works on real hardware.
