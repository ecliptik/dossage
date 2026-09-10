# Cutting a release

What a DOSSAGE release description needs to contain, and how to actually
publish one. Written after v1.0.0's release notes needed two rounds of
correction (hard-wrapped source text that didn't reflow on either
Forgejo or GitHub, and a first draft that was too thin for a first
tagged release) -- this doc exists so the next release starts from the
right shape instead of repeating that.

## What the description must contain

In this order, as level-3 (`###`) headings:

1. **Intro** -- one paragraph, no heading. What DOSSAGE is, linking
   [Passage's own project page](https://hcsoftware.sourceforge.net/passage/)
   (not just naming Jason Rohrer in prose), plus "First tagged release"
   or, for a later release, what's different since the last tag.
2. **Features** -- a short bullet list of what the port actually does
   (protected-mode DOS, SDL3 DOS backend, the custom software-synth
   audio, the custom `.tga` decoder, ships the complete public-domain
   game, DOSBox-X + real-hardware validation). Copy this from the
   README's own intro/Status prose rather than re-inventing it each
   time -- keep the two in sync.
3. **Performance (real hardware)** -- the *actual* current benchmark
   table (CPU x video card, fps), not just a one-line "N/N PASS"
   summary. Pull the live numbers from `docs/benchmarks/README.md`, not
   from a stale copy -- if a new benchmark round has run since the last
   release, the table changes and this section must reflect that.
   Explain *why* the numbers cluster the way they do (Passage's own
   15fps pacer ceiling) rather than just listing them.
4. **Fixed** (only when there's something to report -- omit entirely
   for a release with no behavior changes) -- one bullet per real fix,
   each stating the concrete symptom, the mechanism, and how it was
   confirmed. "Real-hardware confirmed: X" beats "improved reliability."
5. **Licensing** -- one paragraph recapping the actual basis (public
   domain engine + assets, zlib SDL3, DJGPP's no-restriction-when-
   unmodified stance + the GCC Runtime Library Exception, no copyleft
   obligation on the binary), linking `THIRD-PARTY.md` for the full
   matrix. Skip this section only if a later release's notes would be
   pure repetition of an unchanged basis -- for the first release it is
   never optional.
6. **Download** -- what's in the zip, one sentence pointing at
   `README.TXT` inside the archive for the actual quickstart rather
   than repeating it here.

## Style

- **No manual line-wrapping inside a paragraph or bullet.** Both
  Forgejo and GitHub render a single `\n` within a paragraph as a hard
  break, not a reflow -- v1.0.0's first draft hard-wrapped every line
  at ~72 characters in the source and it rendered as jagged, obviously-
  wrapped text on both hosts. Write each paragraph and each bullet's
  text as one continuous line in the source; let the viewer wrap it.
  Verify after publishing with `gh api repos/ecliptik/dossage/releases/tags/<tag> --jq '.body'`
  and check for embedded `\n` inside what should be one paragraph --
  don't trust a rendered screenshot/fetch alone, since a stale page
  cache can show you the previous version and look like confirmation
  of a fix that didn't actually land.
- **Concrete facts, not adjectives.** "Confirmed on real Mach64
  hardware: clean 640x480, no corruption" beats "extensively validated
  for reliability." A number, a before/after, or a specific log line
  beats a qualitative claim every time -- this project's own git
  history is full of exactly these numbers; use them instead of
  paraphrasing them into marketing language.
- **Plain section names.** `### Fixed`, `### Features`, `### Download`
  -- not "### 🎉 What's New" or a banner-style opening line. Look at
  `doskutsu`'s own release notes (`gh release view --repo
  ecliptik/doskutsu`) for the house tone if in doubt.
- **Link the actual docs, not a paraphrase of them.** Full benchmark
  methodology and per-cell logs belong as a link to
  `docs/benchmarks/README.md`, not restated in the release body beyond
  the summary table itself.

## Publishing (both Forgejo and GitHub)

Forgejo is `origin` and push-mirrors to GitHub automatically
(`sync_on_commit: true`, `branch_filter: "main, release/*"`) -- code and
tags reach GitHub on their own. The release *object* (notes + attached
zip) does not mirror and has to be created on each host separately, from
the same source file, since the two hosts need different relative-link
bases (`/blob/<tag>/` on GitHub, `/src/tag/<tag>/` on Forgejo).

1. **Tag and push:**
   ```
   git tag -a vX.Y.Z -m "..."
   git push origin vX.Y.Z
   ```
2. **Build the archive from the exact tagged commit** -- don't reuse a
   build from earlier in the session, rebuild clean so the shipped
   binary is provably the tag, not "close to it":
   ```
   git status --short   # must be clean, HEAD == the tag
   rm -f vendor/passage/gameSource/music/SONG.WAV
   APPLY_PATCHES_FORCE=1 ./scripts/apply-patches.sh
   make render-music AUDIO_TIER=high
   make game AUDIO_TIER=high
   rm -rf build/stage dist
   make dist AUDIO_TIER=high
   ```
3. **Verify the archive before publishing it, every time:**
   ```
   unzip -l dist/dossage.zip
   ```
   Confirm exactly: `DOSSAGE.EXE`, `CWSDPMI.EXE`, `CWSDPMI.DOC`,
   `LICENSE.TXT`, `3RDPARTY.TXT`, `README.TXT`, `BUILDSHA.TXT`,
   `graphics/`, `music/`, `settings/` -- and **nothing else**. `STAGE_DIR`
   is also this port's DOSBox-X/real-hardware mount root, so it
   accumulates `LOGS/`, `RUNMANI.LOG`, `CWSDPMI.SWP`, and DOSBox-X's own
   `.DBLOCALFILE_ATR_*` marker across test runs -- `make dist` strips
   these, but that stripping is exactly the kind of thing a future edit
   to the target could silently break. `unzip -l` and actually read the
   list; don't assume the target still does what its comment says.
4. **Write the notes once**, as a real file (not a shell heredoc --
   heredocs are exactly how the hard-wrap bug happened), then derive a
   per-host copy by substituting the link base:
   ```
   sed 's|__REPO_URL__|https://github.com/ecliptik/dossage/blob/vX.Y.Z|g' \
       notes.md > notes-github.md
   sed 's|__REPO_URL__|https://forgejo.ecliptik.com/ecliptik/dossage/src/tag/vX.Y.Z|g' \
       notes.md > notes-forgejo.md
   ```
5. **GitHub** (needs `gh auth status` to show a valid login first):
   ```
   gh release create vX.Y.Z dist/dossage.zip \
     --repo ecliptik/dossage --title "vX.Y.Z" --notes-file notes-github.md
   ```
6. **Forgejo** (needs a token with write access to this repo, e.g.
   `~/.forgejo_token`):
   ```
   TOKEN=$(cat ~/.forgejo_token)
   JSON_BODY=$(python3 -c "import json,sys; print(json.dumps(sys.stdin.read()))" < notes-forgejo.md)
   curl -s -X POST -H "Authorization: token $TOKEN" -H "Content-Type: application/json" \
     "https://forgejo.ecliptik.com/api/v1/repos/ecliptik/dossage/releases" \
     -d "{\"tag_name\":\"vX.Y.Z\",\"target_commitish\":\"main\",\"name\":\"vX.Y.Z\",\"body\":$JSON_BODY,\"draft\":false,\"prerelease\":false}"
   # then, with the returned release id:
   curl -s -X POST -H "Authorization: token $TOKEN" \
     "https://forgejo.ecliptik.com/api/v1/repos/ecliptik/dossage/releases/<id>/assets?name=dossage.zip" \
     -F "attachment=@dist/dossage.zip"
   ```
7. **Verify both landed correctly** by reading the stored body back via
   API/`gh api ... --jq '.body'` (not a web fetch -- see the style note
   above on stale page caches), and confirm the asset size on each host
   matches `dist/dossage.zip`'s actual size.
