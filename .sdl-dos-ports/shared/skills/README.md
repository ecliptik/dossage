# Skill templates

Genericized Claude Code skills, same adoption model as `shared/agents/`
(see its own `README.md`): these are process templates distilled from real
campaign experience on one port, stripped of that port's specific numbers
and names, meant to be copied (or symlinked) into a port repo's own
`.claude/skills/` rather than invoked from inside this hub repo directly.

A skill here only helps if a port actually has it wired up locally --
this hub is a source of truth for the *content*, not a place these run
from day to day.

| Skill | What it covers | Reusable as-is vs. needs a per-port fill-in |
|---|---|---|
| `dos-hardware-validation/` | Running a real-hardware validation or A/B benchmark campaign on a vcctrl-controlled rig: design-before-run discipline, populate/verify, per-cell set/forbid/expect_log witnessing, RUNMANIFEST-driven metrics, ABBA methodology, bug-vs-workaround discipline, raw-data handoffs | Reusable close to as-is -- the harness protocol and methodology are port-agnostic. Fill in your own RUNMANIFEST field names/env-var names if you don't reuse `DOS_PORT_*` (see `docs/patch-conventions.md`'s naming-debt note), and your own vcctrl profile per `templates/vcctrl-profile.yaml.template` |
| `dos-rig-operations/` | Day-to-day rig mechanics independent of a full campaign: input-injection landed-vs-dropped detection, screen-capture evidence discipline, file transfer + packaging/handoff, log collection, power management, and multi-agent coordination (a spawned specialist doesn't inherit rig access; single-coordinator-per-campaign to prevent double-dispatch) | Reusable as-is -- mostly points at vcctrl's own `vcctrl-rig-hazards`/`vcctrl-common-workflows` for mechanics and adds the port-session framing/hazard classes on top |
| `dos-realhw-verification/` | Knowing a build/fix/diagnosis is actually correct on real hardware, not just apparently correct: two-witness build verification, stale-cache failure shapes, DOSBox-X/86Box tiering, build-host tooling traps, real-hardware-vs-emulator divergence debugging | Reusable as-is -- the epistemics (what a check's failure would look like) and failure-shape catalog are port-agnostic; the worked examples are illustrative, not something to copy literally |
| `dos-emulator-workflow/` | Local, no-rig-required DOSBox-X development: which of the three `shared/tools/dosbox-*.sh` scripts to reach for, the emulator-only escape hatches already baked into the shipped confs (and why they must never reach real hardware), the "necessary but not sufficient" pattern for a probe that can only partially answer a hardware question locally, and local-timing observation vs. an actual performance claim | Reusable as-is for the tooling/mechanics and the DOSBox-X/hardware boundary; the shipped `.conf` files' own `cycles=`/video/sound calibration is one port's reference-machine numbers, not a universal setting -- re-calibrate for your own port |
| `review/` | Checking a patch (`shared/patches/` or a port's own `patches/<engine>/`) against this hub's landing conventions before committing: provenance verification, DJGPP hard constraints, neutral naming, slot numbering, temporary-diagnostic removal planning | Reusable as-is -- invoke as `/review` once symlinked in |
| `benchmark/` | Running a real-hardware performance KPI campaign: KPI writing, team shape, investigation/rig discipline, recording the result | Reusable as-is -- invoke as `/benchmark` once symlinked in |

## Adopting a skill into a port repo

**Preferred, self-contained: `npx skills`.** New port repos get every
current entry from this hub *and* vcctrl automatically --
`scripts/new-port.sh` runs `npx skills add <repo> --full-depth --all -a
claude-code` against both at scaffold time (verified working over Forgejo,
HTTPS or SSH; `--full-depth` is required since neither repo has a
root-level `SKILL.md`). No subtree/submodule symlink needed for skills
specifically -- only the actual platform code (`shared/patches/`,
`shared/build/`, `shared/include/runmanifest.h`, `shared/agents/`) still
needs `.sdl-dos-ports/` (a subtree by default since 2026-08-31, a
submodule for an older port), since `npx skills` only moves `SKILL.md`
content. A port that already has skills installed this way picks up new
ones with `npx skills update`.

**Skills are knowledge; real-hardware access is a separate, deliberate
step.** `scripts/new-port.sh` will also register vcctrl's MCP daemon
automatically *if* `VCCTRL_MCP_URL` is set in the environment when it
runs -- but there is no default for that variable anywhere in this repo,
unlike `VCCTRL_REMOTE` above, and there never will be: the rig's real
MCP endpoint is a secret vcctrl's own operator deliberately keeps out of
every tracked file (mirroring why `vcctrl.yaml` itself stays gitignored
on their side), and that boundary is theirs to hold, not this hub's to
cross by baking a literal into a committed script. Left unset (the
default), registration is skipped cleanly -- see vcctrl's own
`MCP-SERVER.md` sec 5 for manual registration once you actually have the
real URL. This is intentional: a fresh port repo gets vcctrl's
*knowledge* automatically, not ungated *access* to physical hardware.

A port scaffolded *before* this existed (or before a skill was added here)
can adopt it the same way, from that port's own root:

```sh
npx skills add https://forgejo.ecliptik.com/ecliptik/sdl-dos-ports.git --full-depth --all -a claude-code
npx skills add https://forgejo.ecliptik.com/ecliptik/vcctrl.git --full-depth --all -a claude-code
```

**Fallback, if `npx`/network isn't available: the old symlink method.**
Re-run `.sdl-dos-ports/shared/scripts/sync-skills.sh` from that port's own
root to add any missing symlinks for this hub's own skills only (no
vcctrl skills this way; idempotent, safe to re-run, never touches an
existing symlink or a deliberate copy). Equivalent by hand for one skill:

```sh
mkdir -p .claude/skills
ln -s ../.sdl-dos-ports/shared/skills/dos-hardware-validation .claude/skills/dos-hardware-validation
ln -s ../.sdl-dos-ports/shared/skills/dos-rig-operations .claude/skills/dos-rig-operations
ln -s ../.sdl-dos-ports/shared/skills/dos-realhw-verification .claude/skills/dos-realhw-verification
ln -s ../.sdl-dos-ports/shared/skills/dos-emulator-workflow .claude/skills/dos-emulator-workflow
ln -s ../.sdl-dos-ports/shared/skills/review .claude/skills/review
ln -s ../.sdl-dos-ports/shared/skills/benchmark .claude/skills/benchmark
```

Symlinking (rather than copying) keeps the skill in sync with this hub the
same way `patches/SDL` and the vendor scripts do -- see
`plans/DOSKUTSU-MIGRATION-PLAN.md` for the precedent and its one caveat:
anything that enumerates files by walking a symlinked directory needs `-L`
(GNU `find`) or the equivalent, or it silently sees nothing. Copying
instead of symlinking is fine too if a port wants to diverge (e.g. fill in
port-specific RUNMANIFEST field names inline rather than parameterizing) --
same tradeoff as the agent charter templates: copy-and-fill-in loses the
free sync, symlink-and-parameterize keeps it.

## Provenance

`dos-hardware-validation/` and `dos-rig-operations/` were distilled from
the vcctrl side of a real multi-hour real-hardware session validating
doskutsu on the vcctrl rig (Gateway 2000 / POD-83, ATI Mach64) during
doskutsu's `shared/` submodule migration -- an 8-cell ABBA fps campaign,
four real harness bugs found and fixed along the way, and vcctrl's own
prioritized read on what else in its rig-operations surface was worth
generalizing.

`dos-realhw-verification/` was distilled from doskutsu's side of the same
migration window -- its own build/QA and hardware-vs-emulator debugging
campaign history, including a fix that shipped twice on unconfirmed
static hypotheses before real-hardware markers refuted them, and a
multi-round investigation that initially misattributed a bug to a video
chipset before real-hardware surface dumps found the actual cause in the
port's own fast-path code.

Port-specific details (doskutsu's own env var names, the specific fps
numbers, specific bug tickets) were stripped the same way the agent
charter templates strip doskutsu's wave-by-wave findings -- see
`shared/agents/README.md`'s framing. The underlying discipline is what's
shared; the campaign history belongs to whichever port lived it.

`dos-emulator-workflow/` differs in provenance from the other three: it
wasn't distilled from a single incident, but extracted from gotchas
already documented inline in this hub's own `shared/tools/dosbox-*.sh`
comments and `shared/tests/dpmi-lfn-smoke/README.md` (the global-`pkill`
hazard, the `2>&1`-under-DOSBox-X caveat, the `SDL_DOS_AUDIO_SB_SKIP_DETECTION`
escape hatch, the "necessary but not sufficient" probe framing) --
material that was already real and already correct, just not yet
surfaced as something a session would actually consult before starting
local DOSBox-X work.
