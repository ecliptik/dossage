# sdl-dos-ports

A hub for porting SDL-based games to MS-DOS, built on the SDL3 DOS backend
proven by [doskutsu](https://github.com/ecliptik/doskutsu) (Cave Story on
real 486/Pentium-class DOS hardware via SDL3, DJGPP, and CWSDPMI).

This repository does not contain the games themselves. Each port lives in
its own git repository and pulls in the reusable platform layer from
[`shared/`](shared/) as a git subtree at `.sdl-dos-ports/` (a submodule
for a port scaffolded before 2026-08-31 -- both are valid). This repo
tracks status, hosts that shared layer, documents the porting strategy
and hardware/QA process, and showcases finished ports.

See [`CLAUDE.md`](CLAUDE.md) for the rules an AI agent (or contributor)
should follow when working in this repo.

## Ports

| Priority | Project | Upstream | License | Port repo | Status | Difficulty |
|---:|---|---|---|---|---|---:|
| 0 | doskutsu (Cave Story) | [nxengine/nxengine-evo](https://github.com/nxengine/nxengine-evo) | GPL-3.0 (verified) | [ecliptik/doskutsu](https://github.com/ecliptik/doskutsu) | RELEASE_READY | reference |
| 1 | Adventure Game Studio | [adventuregamestudio/ags](https://github.com/adventuregamestudio/ags) | unverified | unclaimed | BACKLOG | 5 |
| 2 | VVVVVV | [TerryCavanagh/VVVVVV](https://github.com/TerryCavanagh/VVVVVV) | unverified — see caution below | unclaimed | BACKLOG | 3 |
| 3 | Meritous | TBD | unverified | unclaimed | BACKLOG | 2 |
| 4 | OpenJazz | [OSSGames/GAME-SDL-openjazz](https://github.com/OSSGames/GAME-SDL-openjazz) | unverified | unclaimed | BACKLOG | 2-3 |
| 5 | POWDER | TBD | unverified — may not be open source, see `ports.yaml` | unclaimed | BACKLOG | 2 |
| 6 | Kobo Deluxe | TBD | unverified | unclaimed | BACKLOG | 3-4 |
| 7 | Blob Wars: Metal Blob Solid | TBD | unverified | unclaimed | BACKLOG | 3-4 |
| 8 | Passage | [jasonrohrer/Passage](https://github.com/jasonrohrer/Passage) | public domain (verified) | [ecliptik/dossage](https://forgejo.ecliptik.com/ecliptik/dossage) | OPTIMIZING | 1 |
| 9 | SuperTux 0.1.x | TBD | unverified | unclaimed | BACKLOG | 4 |

This table is generated from [`ports.yaml`](ports.yaml) — the machine-
readable source of truth, including `upstream_license_verified` per entry.
"Unverified" means exactly that: nobody has yet read the actual upstream
LICENSE/COPYING file for that project in this repo. **Never treat an
unverified license as known** — see [`docs/licensing.md`](docs/licensing.md)
and `ports.yaml`'s header comment. VVVVVV in particular has historically
been distributed under source-available terms that are not automatically
redistribution rights; see its `ports.yaml` entry before assuming anything.

See [`COMPATIBILITY.md`](COMPATIBILITY.md) for a richer boots/playable/
hardware-target matrix as ports progress, and [`gallery/`](gallery/) for
screenshots, performance notes, and features per finished port.

## Getting started

```sh
# 1. Claim a candidate and scaffold its repo (run from this hub repo)
Use the port skill: /sdldos:port

# 2. Open the new port repo and trust it -- Claude Code offers the sdldos
#    plugin (this hub's skills, namespaced) from the repo's own
#    .claude/settings.json; vcctrl's skills are already installed flat
cd ../<name>
ls .claude/skills/          # vcctrl-* (flat); this hub's are /sdldos:*

# 3. Build in narrow slices: compile -> video -> input -> filesystem
#    -> audio -> gameplay. See PORTING.md.

# 4. Chasing a real-hardware fps/perf target?
Use the benchmark skill: /sdldos:benchmark

# 5. Landing a patch in shared/ or your own patches/<engine>/?
Use the review skill: /sdldos:review
```

| Skill | Runs from | Use it to |
|---|---|---|
| `/sdldos:port` | This hub repo only | Claim a `BACKLOG` candidate and scaffold its repo, charters, and vendor pins. |
| `/sdldos:review` | This hub, or any port repo | Check a patch against this hub's own landing conventions before committing it (provenance, DJGPP constraints, naming, numbering). |
| `/sdldos:benchmark` | This hub, or any port repo | Run or record a real-hardware performance KPI campaign. |
| `/sdldos:dos-emulator-workflow`, `/sdldos:dos-hardware-validation`, `/sdldos:dos-rig-operations`, `/sdldos:dos-realhw-verification` | Any port repo | Local DOSBox-X work, rig campaigns, day-to-day rig mechanics, and real-hardware verification discipline. |

**How the skills get there.** This repo is a Claude Code plugin
(`.claude-plugin/plugin.json`, name `sdldos`; the root `skills/` directory
is its skill set). Every port repo scaffolded by `/sdldos:port` carries a
tracked `.claude/settings.json` that registers this hub as a plugin
marketplace and enables the plugin, so anyone who opens and trusts a port
repo is offered every skill above under the `sdldos:` prefix, with no
per-repo copy of any `SKILL.md`. This hub's own `.claude/settings.json`
does the same for sessions here. To install it by hand anywhere else:

```sh
/plugin marketplace add https://forgejo.ecliptik.com/ecliptik/sdl-dos-ports.git
/plugin install sdldos@sdl-dos-ports
```

The installed plugin is a snapshot keyed by the `version` in
`.claude-plugin/plugin.json`: `/plugin update sdldos` fetches a new
version and says "already at the latest version" otherwise, so a skill
or layout change here must bump that version to reach installed copies.
`claude --plugin-dir <path-to-this-hub>` loads the working tree for one
session instead (it overrides the installed copy), which is how to try a
skill edit here before pushing it. vcctrl's
skills are not a plugin; `/sdldos:port` installs them flat into the new
repo via `npx skills`. A port scaffolded before the plugin existed, or an
agent that can't load plugins (Codex, Cursor), is covered in
`shared/skills/README.md`.

Deeper reference, only when a skill points you at it:
[`PORTING.md`](PORTING.md) for the full slice-by-slice porting process.

## Repository layout

```
sdl-dos-ports/
├── ports.yaml         # candidate backlog + status tracker (source of truth)
├── docs/              # architecture, video/audio/input/filesystem/timing, testing, licensing
├── shared/            # the reusable SDL3-DOS platform layer -- port repos consume this via subtree
├── templates/         # scaffolding for a new port repo (STATUS/PLAN/CLAUDE/benchmark/license-review templates)
├── gallery/            # per-port showcase: screenshots, performance, features
└── scripts/           # scripts/new-port.sh and friends
```

## Reference hardware

Primary targets, shared across all ports (see [`HARDWARE.md`](HARDWARE.md)
for the full matrix and real-hardware testing process via
[vcctrl](https://github.com/ecliptik/vcctrl)):

| ID | CPU | Role |
|---|---|---|
| HW-486-50 | 486DX2-50 | low-end torture test |
| HW-486-66 | 486DX2-66 | primary minimum target |
| HW-POD83 | Pentium OverDrive 83 | upgrade-path target |
| HW-5X86 | AMD Am5x86-133 | fast 486 platform |
| HW-P75 | Pentium 75 | recommended target |

## License

See [`LICENSE`](LICENSE) for this repository's own code and docs, and
[`THIRD-PARTY.md`](THIRD-PARTY.md) for the licensing model applied to
vendored/patched third-party sources in `shared/`. Every port repo tracks
its own game engine and asset licensing separately in its own
`LICENSE-REVIEW.md`.
