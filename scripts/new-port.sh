#!/usr/bin/env bash
# Scaffold a new port repository from templates/, wired to this repo's
# shared/ layer as a git subtree at .sdl-dos-ports/. See PORTING.md.
#
# Why a subtree, not a submodule (changed 2026-08-31): a subtree merges
# the hub's content directly into the port repo's own git history at the
# same .sdl-dos-ports/ prefix a submodule used -- every existing
# .sdl-dos-ports/shared/..., .sdl-dos-ports/docs/... reference in this
# hub's own skills/docs/scripts keeps working unchanged, since the path
# doesn't move. What changes is the mechanism: no .gitmodules, no
# separate nested checkout, no "did you run --init --recursive" class of
# bug -- a plain `git clone` of a port repo gets .sdl-dos-ports/ content
# immediately. The reproducibility anchor a submodule gitlink provided
# is, if anything, more visible this way: `git subtree add/pull --squash`
# writes the exact hub commit SHA it pulled from directly into a
# human-readable commit message (`git log` shows it plainly), rather than
# a gitlink object needing `git submodule status` to inspect. Verified
# end to end (add, pull-to-update, idempotent-detection) before this
# script was changed to use it.
#
# Usage:
#   scripts/new-port.sh <name> [target-dir]
#
# <name>       lowercase-hyphenated, must match a candidate in ports.yaml
#              (or you're about to add one -- do that first).
# [target-dir] where to create the new repo. Defaults to ../<name>
#              (a sibling of this repo), matching the "each port is its
#              own repo" model.
#
# This script does not push anywhere. It sets up a local repo with the
# shared/ subtree already added and templates already filled in; review
# and push it yourself.

set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: $0 <name> [target-dir]" >&2
  exit 1
fi

NAME="$1"
HUB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="${2:-${HUB_DIR}/../${NAME}}"

if [ -e "$TARGET_DIR" ]; then
  echo "error: $TARGET_DIR already exists" >&2
  exit 1
fi

# Prefer this repo's own configured remote for the subtree source URL, so
# a fork or a not-yet-pushed local clone still works; fall back to the
# known forgejo location otherwise.
HUB_REMOTE="$(git -C "$HUB_DIR" remote get-url origin 2>/dev/null || true)"
HUB_REMOTE="${HUB_REMOTE:-ssh://git@forgejo.ecliptik.com/ecliptik/sdl-dos-ports.git}"

echo "==> Creating $TARGET_DIR"
mkdir -p "$TARGET_DIR"
cd "$TARGET_DIR"
git init -q

# git subtree add requires at least one commit to exist in the target repo
# (an empty tree has no HEAD to merge onto) -- this initial commit is
# otherwise inert and gets superseded by the template/scaffold commits
# below in the operator's own review before pushing.
git commit -q --allow-empty -m "chore: init"

echo "==> Adding sdl-dos-ports as a subtree at .sdl-dos-ports/ ($HUB_REMOTE)"
git subtree add --prefix=.sdl-dos-ports -q "$HUB_REMOTE" main --squash || {
  echo "warning: subtree add failed (offline / remote not yet pushed?)." >&2
  echo "         Falling back to a local path subtree against $HUB_DIR." >&2
  git subtree add --prefix=.sdl-dos-ports -q "$HUB_DIR" main --squash
}

echo "==> Creating directory skeleton"
mkdir -p vendor patches scripts tests qa-results setup profiles .claude/agents .claude/skills

echo "==> Wiring scripts/ convenience symlinks into .sdl-dos-ports/shared/scripts/"
# Matches the existing convention in doskutsu and dossage: PORTING.md and
# this script's own closing message tell an operator to run
# ./scripts/setup-symlinks.sh and ./scripts/fetch-sources.sh directly, so
# those need to exist from the first commit, not just be reachable via the
# longer .sdl-dos-ports/shared/scripts/ path.
ln -s ../.sdl-dos-ports/shared/scripts/fetch-sources.sh scripts/fetch-sources.sh
ln -s ../.sdl-dos-ports/shared/scripts/apply-patches.sh scripts/apply-patches.sh
ln -s ../.sdl-dos-ports/shared/scripts/setup-symlinks.sh scripts/setup-symlinks.sh
ln -s ../.sdl-dos-ports/shared/scripts/verify-patches-applied.sh scripts/verify-patches-applied.sh

echo "==> Vendoring the SDL3-DOS patch series into patches/ (real files, not symlinks)"
# A new port gets its OWN copy of the hub's current patch series -- this is
# a one-time seed, not an ongoing link. The copy is real, port-owned files
# from the start (never symlinked into .sdl-dos-ports/), so `git clone` of
# just this port repo builds standalone -- no subtree/submodule reach-back
# needed for patches specifically, only for the rest of shared/ (build
# fragments, runmanifest.h, midi_sched, agents, tools). A later hub
# improvement to the reference series does not reach an already-scaffolded
# port automatically; porting it over is a deliberate, reviewed act, same
# as porting a fix between any two independent repos. See
# docs/patch-conventions.md.
mkdir -p patches/SDL patches/SDL_mixer
cp "$HUB_DIR"/shared/patches/sdl3-dos/*.patch patches/SDL/
cp "$HUB_DIR"/shared/patches/sdl3-mixer/*.patch patches/SDL_mixer/

# VCCTRL_REMOTE follows the same override convention as other hub-location
# vars in this codebase (EMULATORS_ROOT, SDL_DOS_PORT_ROOT) -- set it if
# vcctrl lives somewhere other than the default forgejo location.
VCCTRL_REMOTE="${VCCTRL_REMOTE:-https://forgejo.ecliptik.com/ecliptik/vcctrl.git}"

echo "==> Installing skills (sdl-dos-ports + vcctrl) via npx skills"
NPX_SKILLS_OK=1
if ! command -v npx >/dev/null 2>&1; then
  echo "    npx not found -- skipping, falling back to the hub-only symlink method." >&2
  NPX_SKILLS_OK=0
elif ! npx --yes skills@latest add "$HUB_REMOTE" --full-depth --all -a claude-code -y >/dev/null 2>&1; then
  echo "    npx skills add failed for sdl-dos-ports (offline?) -- falling back to the hub-only symlink method." >&2
  NPX_SKILLS_OK=0
elif ! npx --yes skills@latest add "$VCCTRL_REMOTE" --full-depth --all -a claude-code -y >/dev/null 2>&1; then
  echo "    warning: sdl-dos-ports skills installed, but vcctrl skills failed (offline? VCCTRL_REMOTE wrong?)." >&2
  echo "             Re-run manually: npx skills add $VCCTRL_REMOTE --full-depth --all -a claude-code" >&2
fi

if [ "$NPX_SKILLS_OK" -eq 0 ]; then
  echo "==> Symlinking shared skills into .claude/skills/ (hub only -- no vcctrl skills this way)"
  for skill_dir in .sdl-dos-ports/shared/skills/*/; do
    skill_name="$(basename "$skill_dir")"
    ln -s "../../.sdl-dos-ports/shared/skills/${skill_name}" ".claude/skills/${skill_name}"
  done
fi

# VCCTRL_MCP_URL is intentionally NOT a hardcoded default anywhere in this
# repo, unlike VCCTRL_REMOTE above -- the rig's real MCP endpoint is a
# secret vcctrl's own operator deliberately keeps out of every tracked
# file (same reason vcctrl.yaml itself stays gitignored on that side; see
# docs/MCP-SERVER.md's usb4vc.example.ts.net placeholder). This script
# never learns or stores the real value -- whoever runs the scaffold
# supplies it locally in their own shell environment if they want
# unattended registration. Registration is skipped cleanly, not an
# error, when it's unset.
if [ -n "${VCCTRL_MCP_URL:-}" ]; then
  echo "==> Registering vcctrl MCP (daemon mode) via VCCTRL_MCP_URL"
  if ! command -v claude >/dev/null 2>&1; then
    echo "    'claude' CLI not found -- skipping. Register manually later:" >&2
    echo "    claude mcp add --transport http vcctrl-mcp-daemon \"\$VCCTRL_MCP_URL\"" >&2
  elif ! claude mcp add --transport http vcctrl-mcp-daemon "$VCCTRL_MCP_URL" >/dev/null 2>&1; then
    echo "    warning: MCP registration failed -- register manually, see docs/MCP-SERVER.md sec 5." >&2
  else
    echo "    registered. Restart the session in this repo to pick it up (MCP doesn't hot-reload)."
  fi
else
  echo "==> Skipping vcctrl MCP registration (VCCTRL_MCP_URL not set)"
  echo "    Real-hardware rig access is not auto-granted -- this is deliberate, not a gap."
  echo "    See .sdl-dos-ports's vcctrl MCP-SERVER.md sec 5 for the manual one-line registration"
  echo "    once you have the rig's real MCP URL, or export VCCTRL_MCP_URL before re-running this"
  echo "    script to register automatically next time."
fi

echo "==> Filling in templates"
NAME_UPPER="$(echo "$NAME" | tr '[:lower:]-' '[:upper:]_')"

sed -e "s/<NAME>/${NAME}/g" -e "s/<name>/${NAME}/g" \
  ".sdl-dos-ports/templates/PORT-README.md" > README.md
sed -e "s/<NAME>/${NAME}/g" -e "s/<name>/${NAME}/g" \
  ".sdl-dos-ports/templates/PORT-STATUS.md" > STATUS.md
sed -e "s/<NAME>/${NAME}/g" -e "s/<name>/${NAME}/g" \
  ".sdl-dos-ports/templates/PORT-PLAN.md" > PLAN.md
sed -e "s/<NAME>/${NAME}/g" -e "s/<name>/${NAME}/g" \
  ".sdl-dos-ports/templates/PORT-CLAUDE.md" > CLAUDE.md
sed -e "s/<NAME>/${NAME}/g" -e "s/<name>/${NAME}/g" \
  ".sdl-dos-ports/templates/LICENSE-REVIEW.md" > LICENSE-REVIEW.md
sed -e "s/<NAME>/${NAME_UPPER}/g" -e "s/<name>/${NAME}/g" \
  ".sdl-dos-ports/templates/vcctrl-profile.yaml.template" > "profiles/${NAME}.yaml"

cat > vendor/sources.manifest <<'EOF'
# vendor/sources.manifest -- pinned upstream sources for this port.
# Format: <name>  <url>  <ref>  <sha>
# See .sdl-dos-ports/docs/patch-conventions.md.
#
# This port vendors its OWN copy of SDL/SDL_mixer/SDL_image source, AND
# its own copy of the DOS patch series (patches/SDL, patches/SDL_mixer --
# real files, seeded once from the hub at scaffold time, not symlinked --
# see .sdl-dos-ports/docs/patch-conventions.md). Fill in real pinned SHAs
# for the three entries below (match whatever SHA patches/SDL's own patch
# headers/README were built against), and add this port's own engine as a
# fourth entry.
SDL            https://github.com/libsdl-org/SDL.git          main          PIN_ME
SDL_mixer      https://github.com/libsdl-org/SDL_mixer.git    release-3.2.x PIN_ME
SDL_image      https://github.com/libsdl-org/SDL_image.git    release-3.2.x PIN_ME
EOF

echo "==> Done."
echo ""
echo "Next steps:"
echo "  1. Fill in README.md, STATUS.md, PLAN.md, LICENSE-REVIEW.md, and"
echo "     profiles/${NAME}.yaml (placeholders are marked <...>)."
echo "  2. Locate the canonical upstream repo and pin a revision in"
echo "     vendor/sources.manifest -- never guess."
echo "  3. Copy .sdl-dos-ports/shared/agents/*.md into .claude/agents/ and"
echo "     fill in the placeholders (see shared/agents/README.md's table)."
echo "  4. Set this repo's own git remote and push when ready."
echo "  5. In the sdl-dos-ports hub repo, update ports.yaml: set"
echo "     dos_status: RESEARCH and port_repo_url for '${NAME}'."
echo ""
echo "(Steps 1-3 plus the upstream/license research and the handoff to a"
echo "porting agent are exactly what the sdl-dos-ports hub's 'port' skill"
echo "(/port) automates -- run this script directly only if you want just"
echo "the mechanical scaffold.)"
