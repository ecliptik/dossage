# Makefile -- dossage (Passage DOS port)
#
# Orchestrates: SDL3 cross-build (shared fragment) -> the Passage/minorGems
# engine stage (this file). Neither SDL3_mixer nor SDL3_image is vendored
# or linked -- Passage's audio is a from-scratch synth over core SDL3's
# SDL_OpenAudioDeviceStream (no file-decode codecs needed) and its .tga
# assets are decoded by minorGems' own TGAImageConverter (no PNG/libpng
# needed). See vendor/sources.manifest's NOTE entries.

PORT_NAME := dossage
REPO_ROOT := $(abspath .)
HUB_DIR   := $(REPO_ROOT)/.sdl-dos-ports

include $(HUB_DIR)/shared/build/sdl3-dos.mk

# Standalone DJGPP diagnostic probes (tests/probes/*.c) -- separate build
# target, no SDL/engine dependency. See tests/probes/probes.mk and
# tests/probes/README.md.
include $(REPO_ROOT)/tests/probes/probes.mk

.PHONY: all
all: sdl3 game

# Install this repo's git hooks. core.hooksPath is local config and is not
# carried by a clone, so a fresh checkout has to run this once. The hook
# refuses commits that edit the .sdl-dos-ports/ subtree -- those never
# reach the hub and the next `git subtree pull` silently reverts them.
# See docs/hub-and-vendoring.md.
.PHONY: hooks
hooks:
	@git config core.hooksPath .githooks
	@echo "git hooks installed (core.hooksPath=.githooks)"

# --- Game stage: Passage + the vendored minorGems subset -------------------

PASSAGE_SRC  := $(VENDOR_DIR)/passage/gameSource
MINORGEMS_SRC := $(VENDOR_DIR)/minorgems

GAME_SOURCES := \
    $(PASSAGE_SRC)/game.cpp \
    $(PASSAGE_SRC)/landscape.cpp \
    $(PASSAGE_SRC)/blowUp.cpp \
    $(PASSAGE_SRC)/World.cpp \
    $(PASSAGE_SRC)/map.cpp \
    $(PASSAGE_SRC)/common.cpp \
    $(PASSAGE_SRC)/score.cpp \
    $(PASSAGE_SRC)/musicPlayer.cpp \
    $(PASSAGE_SRC)/Timbre.cpp \
    $(PASSAGE_SRC)/Envelope.cpp \
    $(MINORGEMS_SRC)/io/file/dos/PathDOS.cpp \
    $(MINORGEMS_SRC)/system/dos/TimeDOS.cpp \
    $(MINORGEMS_SRC)/system/dos/ThreadDOS.cpp \
    $(MINORGEMS_SRC)/util/stringUtils.cpp \
    $(MINORGEMS_SRC)/util/StringBufferOutputStream.cpp \
    $(MINORGEMS_SRC)/util/SettingsManager.cpp \
    $(MINORGEMS_SRC)/crypto/hashes/sha1.cpp \
    $(MINORGEMS_SRC)/formats/encodingUtils.cpp

GAME_OBJECTS := $(patsubst %.cpp,%.o,$(GAME_SOURCES))

# DOS-PORT: build-time audio tier. Real-hardware measurement on the
# port's minimum-target CPU (486DX2-50) found a genuine quality-vs-fps
# tradeoff between full stereo at 22050Hz (fuller sound, but real pump-
# loop cost -- ~9.45fps, matches a direct "occasionally stutters"
# listening report) and mono at 11025Hz (lower fidelity, but the pump
# loop drops to essentially free -- ~10.4-10.6fps). Rather than pick one
# for every build, AUDIO_TIER selects between them:
#   make game                 (AUDIO_TIER=high, the default) -> stereo/22050Hz
#   make game AUDIO_TIER=low  -> mono/11025Hz
# See musicPlayer.cpp's own comment on DOSSAGE_SAMPLE_RATE/
# DOSSAGE_AUDIO_CHANNELS and tools/render-music/render_music.cpp's
# matching tier-aware WAV write. The exact boundary between "high-end"
# and "low-end" target hardware is not yet determined by real data
# (blocked on a hardware-matrix benchmark sweep the operator has
# deferred, 2026-08-27) -- this defaults to the higher tier for now.
AUDIO_TIER ?= high

ifeq ($(AUDIO_TIER),low)
    AUDIO_TIER_FLAGS := -DDOSSAGE_SAMPLE_RATE=11025 -DDOSSAGE_AUDIO_CHANNELS=1
else ifeq ($(AUDIO_TIER),high)
    AUDIO_TIER_FLAGS := -DDOSSAGE_SAMPLE_RATE=22050 -DDOSSAGE_AUDIO_CHANNELS=2
else
    $(error AUDIO_TIER must be 'high' or 'low' (got '$(AUDIO_TIER)'))
endif

# DOS-PORT: %.o's pattern rule below has no dependency-tracking on
# Makefile variables -- switching AUDIO_TIER between invocations without
# a clean first would silently relink stale .o files built with the
# PREVIOUS tier's -D flags. That's a real correctness hazard, not just a
# stale-build annoyance: a channel-count mismatch between the compiled-in
# audioFormat.channels and whatever SONG.WAV actually ships sounds like
# wrong-speed/wrong-pitch playback, not an error message (the exact bug
# class patch 0022 was careful to avoid the first time). This stamp file
# forces every game object to rebuild whenever AUDIO_TIER actually
# changes, and leaves normal incremental rebuilds alone otherwise.
AUDIO_TIER_STAMP := $(BUILD_DIR)/.audio_tier-$(AUDIO_TIER)

$(GAME_OBJECTS): $(AUDIO_TIER_STAMP)

$(AUDIO_TIER_STAMP):
	mkdir -p $(BUILD_DIR)
	rm -f $(BUILD_DIR)/.audio_tier-high $(BUILD_DIR)/.audio_tier-low
	rm -f $(GAME_OBJECTS)
	touch $@

GAME_CXXFLAGS := \
    -I$(VENDOR_DIR) \
    -I$(HUB_DIR)/shared/include \
    -I$(SYSROOT)/include \
    -march=i486 -mtune=pentium -O2 -fno-rtti -fomit-frame-pointer \
    $(AUDIO_TIER_FLAGS) \
    $(NOSIMD_FLAGS)

# --- Build fingerprint -----------------------------------------------------
#
# A content hash of everything that determines the binary, so a benchmark or
# release result can name the build it came from and that name can be
# RECOMPUTED later rather than merely trusted.
#
# This exists because the previous pin could not be. docs/BENCHMARK-PLAN.md
# anchored a whole campaign to build_sha12 = 9cace45a, a value that appears
# nowhere else, is not a git commit, is 8 hex where the convention is 12, and
# has no recorded derivation -- so it could not be checked, only believed.
# See that file's build-pin section.
#
# Hashes the INPUTS, never the output: a fingerprint of the binary that is
# also embedded in the binary is circular and cannot be made to converge.
#
# Covered: the three vendor trees post-patch (which folds in each pin AND its
# full applied patch series), the audio tier, the engine-stage compiler flags,
# the hub build fragment that supplies the SDL stage's own flags, and the
# compiler version.
#
# NOT covered: anything reached only at runtime -- game data under
# gameSource/graphics|music|settings, CWSDPMI, or SDL hints set by the
# launcher. Two builds with the same fingerprint can still behave differently
# if the staged data differs, so hash-verify the staged tree separately (the
# pre-flight in docs/BENCHMARK-PLAN.md already says to).
#
# UPDATE (patches/passage/0034): now passed as -DPORT_BUILD_SHA12 into the
# compile (see the GAME_CXXFLAGS += and BUILD_SHA12_STAMP below) -- the gap
# this comment used to describe ("dossage has no runmanifest emission at
# all, so compiling this in would buy nothing") is closed: game.cpp now
# emits a RUNMANIFEST block at clean exit, reading this macro into the
# block's binary_sha12 field. This DOES alter the binary, so it must not
# land between a baseline build and the run it anchors -- same rule as
# before, just satisfied now rather than deferred.
BUILD_SHA12 := $(shell { \
    git -C $(VENDOR_DIR)/SDL        rev-parse HEAD^{tree}; \
    git -C $(VENDOR_DIR)/passage    rev-parse HEAD^{tree}; \
    git -C $(VENDOR_DIR)/minorgems  rev-parse HEAD^{tree}; \
    echo "AUDIO_TIER=$(AUDIO_TIER)"; \
    echo "GAME_CXXFLAGS=$(GAME_CXXFLAGS)"; \
    sha256sum $(HUB_DIR)/shared/build/sdl3-dos.mk; \
    $(CXX) -dumpversion; \
  } 2>/dev/null | sha256sum | cut -c1-12)

# DOS-PORT: compile the fingerprint into the binary, so a RUNMANIFEST block
# emitted at runtime (patches/passage/0034) can self-report which build
# produced it. Appended via += only AFTER BUILD_SHA12 is fully computed
# above -- BUILD_SHA12's own shell recipe hashes GAME_CXXFLAGS's PRIOR
# (pre-append) value, so baking the fingerprint into the very flags the
# fingerprint hashes would be circular. Make evaluates a simply-expanded
# (:=) variable's += using whatever the referenced variable already holds
# at that point in the file, so this ordering is what makes it non-circular
# -- moving this earlier than BUILD_SHA12's own assignment would break it.
GAME_CXXFLAGS += -DPORT_BUILD_SHA12=\"$(BUILD_SHA12)\"

# DOS-PORT: same stale-.o hazard AUDIO_TIER_STAMP (above) exists to prevent,
# for a different variable -- the %.o pattern rule tracks sources, not
# flags, so a BUILD_SHA12 change (e.g. a vendor patch landing) would
# otherwise silently relink .o files still carrying the PREVIOUS
# fingerprint, making a binary's own self-reported binary_sha12 a lie about
# what produced it. Unlike AUDIO_TIER_STAMP's two fixed names, BUILD_SHA12
# has unbounded possible values, so the stamp's filename embeds the hash
# itself (goes stale, forcing a rebuild, exactly when BUILD_SHA12 changes)
# rather than enumerating prior values to delete.
BUILD_SHA12_STAMP := $(BUILD_DIR)/.build_sha12-$(BUILD_SHA12)

$(GAME_OBJECTS): $(BUILD_SHA12_STAMP)

$(BUILD_SHA12_STAMP):
	mkdir -p $(BUILD_DIR)
	rm -f $(BUILD_DIR)/.build_sha12-*
	rm -f $(GAME_OBJECTS)
	touch $@

BUILD_SHA12_FILE := $(BUILD_DIR)/dossage.build-sha12

.PHONY: build-sha12
build-sha12:
	@echo "$(BUILD_SHA12)"

%.o: %.cpp
	$(CXX) $(GAME_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/dossage.exe: $(SYSROOT)/lib/libSDL3.a $(GAME_OBJECTS)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(GAME_CXXFLAGS) -o $@ $(GAME_OBJECTS) -L$(SYSROOT)/lib -lSDL3 -lm
	$(STUBEDIT) $@ minstack=2048k
	@printf '%s\n' "$(BUILD_SHA12)" > $(BUILD_SHA12_FILE)
	@printf 'build_sha12=%s  sha256(exe)=%s\n' \
	    "$(BUILD_SHA12)" "$$(sha256sum $@ | cut -c1-12)"

.PHONY: game
game: verify-patches-applied $(BUILD_DIR)/dossage.exe

.PHONY: game-clean
game-clean:
	rm -f $(GAME_OBJECTS) $(BUILD_DIR)/dossage.exe

# --- Stage: DOSBox-X mount root ---------------------------------------------
#
# DOSSAGE.EXE + CWSDPMI.EXE + graphics/music/settings sit together, matching
# Passage's own relative-path asset lookup (readTGA("graphics", ...) /
# readTGA("music", ...) / SettingsManager's default "settings" directory).

STAGE_DIR    := $(BUILD_DIR)/stage
CWSDPMI_EXE  := $(REPO_ROOT)/vendor/cwsdpmi/cwsdpmi.exe
# LICENSE REQUIREMENT, not an optional extra: CWSDPMI is freeware and
# redistributable, but its own terms must travel with the binary. See
# LICENSE-REVIEW.md -- "Bundled releases must still include CWSDPMI's own
# redistribution terms (CWSDPMI.DOC, freeware/redistributable) alongside the
# binary." build/stage/ IS what gets copied to a DOS machine and what a
# release archive is cut from, so the .DOC has to be staged with the .EXE or
# every copy made from it is out of compliance.
CWSDPMI_DOC  := $(REPO_ROOT)/vendor/cwsdpmi/cwsdpmi.doc

# DOS-PORT: the WAV's format must match the binary's compiled-in tier.
# musicPlayer.cpp ignores the WAV header and copies raw samples at the
# tier's rate/channel count, so a mismatch is not an error message but
# wrong-speed, wrong-pitch music (a mono 11025Hz file under a 22050Hz
# stereo binary plays 4x fast, two octaves up). The committed SONG.WAV is
# the LOW-tier render (patch 0024), and nothing re-renders it when
# AUDIO_TIER changes -- so a plain `make stage` at the default high tier
# shipped exactly that mismatch (found 2026-09-03, main tree's
# build/stage). Fail loudly instead.
SONG_WAV := $(PASSAGE_SRC)/music/SONG.WAV
ifeq ($(AUDIO_TIER),low)
    SONG_WAV_EXPECT_CH   := 1
    SONG_WAV_EXPECT_RATE := 11025
else
    SONG_WAV_EXPECT_CH   := 2
    SONG_WAV_EXPECT_RATE := 22050
endif

.PHONY: check-song-tier
check-song-tier:
	@test -f "$(SONG_WAV)" || (echo "error: $(SONG_WAV) missing" >&2; exit 1)
	@ch=$$(od -An -tu2 -j22 -N2 "$(SONG_WAV)" | tr -d ' '); \
	 rate=$$(od -An -tu4 -j24 -N4 "$(SONG_WAV)" | tr -d ' '); \
	 if [ "$$ch" != "$(SONG_WAV_EXPECT_CH)" ] || [ "$$rate" != "$(SONG_WAV_EXPECT_RATE)" ]; then \
	     echo "error: $(SONG_WAV) is $${ch}ch/$${rate}Hz but AUDIO_TIER=$(AUDIO_TIER) needs $(SONG_WAV_EXPECT_CH)ch/$(SONG_WAV_EXPECT_RATE)Hz" >&2; \
	     echo "       run: make render-music AUDIO_TIER=$(AUDIO_TIER)   (then re-run make stage)" >&2; \
	     exit 1; \
	 fi; \
	 echo "SONG.WAV: $${ch}ch/$${rate}Hz matches AUDIO_TIER=$(AUDIO_TIER)"

.PHONY: stage
stage: $(BUILD_DIR)/dossage.exe check-song-tier
	@test -f "$(CWSDPMI_EXE)" || (echo "error: $(CWSDPMI_EXE) missing -- run ./scripts/fetch-vendor-binaries.sh" >&2; exit 1)
	@test -f "$(CWSDPMI_DOC)" || (echo "error: $(CWSDPMI_DOC) missing -- required by CWSDPMI's redistribution terms; run ./scripts/fetch-vendor-binaries.sh" >&2; exit 1)
	mkdir -p "$(STAGE_DIR)"
	install -m 0644 $(BUILD_DIR)/dossage.exe "$(STAGE_DIR)/DOSSAGE.EXE"
	install -m 0644 $(CWSDPMI_EXE)           "$(STAGE_DIR)/CWSDPMI.EXE"
	install -m 0644 $(CWSDPMI_DOC)           "$(STAGE_DIR)/CWSDPMI.DOC"
	# The build fingerprint travels with the binary it identifies. The engine
	# has no runmanifest emission, so nothing in a run log names the build --
	# staging this makes the fingerprint readable ON the target instead, and
	# means a staged tree that got mixed up with another build's is
	# detectable rather than silent. 8.3 name: DOS reads this too.
	install -m 0644 $(BUILD_SHA12_FILE)      "$(STAGE_DIR)/BUILDSHA.TXT"
	rm -rf "$(STAGE_DIR)/graphics" "$(STAGE_DIR)/music" "$(STAGE_DIR)/settings"
	cp -r $(PASSAGE_SRC)/graphics "$(STAGE_DIR)/graphics"
	cp -r $(PASSAGE_SRC)/music    "$(STAGE_DIR)/music"
	cp -r $(PASSAGE_SRC)/settings "$(STAGE_DIR)/settings"

# --- Release archive ---------------------------------------------------------
#
# `make dist` packages build/stage/ (already correct and license-compliant
# per the `stage` target above -- DOSSAGE.EXE, CWSDPMI.EXE/.DOC, and the
# graphics/music/settings data) plus three DOS-readable text files into a
# zip anyone can extract straight onto a DOS machine. See LICENSE-REVIEW.md
# and THIRD-PARTY.md's own "Verification" checklist for what belongs here.

DIST_DIR   := $(REPO_ROOT)/dist
DIST_STAGE := $(DIST_DIR)/dossage
DIST_ZIP   := $(DIST_DIR)/dossage.zip

# CRLF filter for DOS-facing text files -- DOS EDIT/TYPE expect CRLF, not
# bare LF (a repo doc copied in as-is reads as one long line on DOS).
CRLF := awk 'BEGIN{ORS="\r\n"} {sub(/\r$$/, ""); print}'

define DIST_README
DOSSAGE - Passage for MS-DOS
============================

DOSSAGE is a port of Jason Rohrer's Passage (2007) to MS-DOS, cross-
compiled with DJGPP against a DOS-ported SDL3.

HOW TO RUN
----------

 1. Copy this whole folder onto your DOS machine, e.g. C:\DOSSAGE\.
    DOSSAGE.EXE, CWSDPMI.EXE, and the graphics/music/settings
    directories must stay together -- the game finds its own assets
    by relative path.
 2. Boot DOS with HIMEM.SYS loaded and NO EMS page frame (DJGPP uses
    DPMI, not EMS).
 3. If you have a Sound Blaster or compatible card, set BLASTER, e.g.
        SET BLASTER=A220 I5 D1 H5 T6
    DOSSAGE runs silently with no sound card at all.
 4. Load a VESA 2.0+ BIOS driver if your video card doesn't provide
    one in its firmware (UniVBE as a fallback).
 5. Run:
        C:\>CD \DOSSAGE
        C:\DOSSAGE>DOSSAGE

Any key dismisses the title screen and starts the game. Arrow keys
move. Q or ESC quits. A full playthrough is about five minutes --
that is the whole point of the piece.

GAME DATA
---------

This bundle includes the complete game: Passage's own graphics and
music are public domain from the same author as the engine, so
nothing further needs to be supplied or extracted.

CWSDPMI
-------

CWSDPMI.EXE is the DPMI host required by DJGPP-compiled programs on
DOS. It must be in the current directory or on PATH when DOSSAGE.EXE
runs. License terms: CWSDPMI.DOC.

LICENSES
--------

This binary carries no copyleft obligation: Passage and its minorGems
dependency are both public domain, and SDL3 is zlib. The port source
code in this repository is MIT licensed. See LICENSE.TXT (this
repo's MIT license plus a note on the public-domain game content) and
3RDPARTY.TXT (the complete attribution matrix).

SOURCE
------

Full source, including build scripts and DOS-port patches:
    @REPO_URL@
endef
export DIST_README

.PHONY: dist
dist: stage
	@test -f LICENSE        || (echo "error: LICENSE missing in repo root" >&2; exit 1)
	@test -f THIRD-PARTY.md || (echo "error: THIRD-PARTY.md missing" >&2; exit 1)
	@rm -rf "$(DIST_STAGE)" "$(DIST_ZIP)"
	@mkdir -p "$(DIST_STAGE)"
	@cp -r "$(STAGE_DIR)/." "$(DIST_STAGE)/"
	@# STAGE_DIR is also this port's own DOSBox-X/real-hardware mount root,
	@# so it accumulates runtime artifacts across test runs (RUNMANI.LOG,
	@# LOGS/, CWSDPMI.SWP, DOSBox-X's own .DBLOCALFILE_ATR_* markers) that
	@# are not part of the game and must never ship in a release archive.
	@rm -rf "$(DIST_STAGE)/LOGS" "$(DIST_STAGE)/RUNMANI.LOG" \
	        "$(DIST_STAGE)/CWSDPMI.SWP" "$(DIST_STAGE)"/.DBLOCALFILE_ATR_*
	@$(CRLF) < LICENSE        > "$(DIST_STAGE)/LICENSE.TXT"
	@$(CRLF) < THIRD-PARTY.md > "$(DIST_STAGE)/3RDPARTY.TXT"
	@url='https://forgejo.ecliptik.com/ecliptik/dossage'; \
	    printf '%s\n' "$$DIST_README" | \
	    awk -v url="$$url" '{gsub(/@REPO_URL@/, url); print}' | \
	    $(CRLF) > "$(DIST_STAGE)/README.TXT"
	@(cd "$(DIST_STAGE)" && zip -q -r "$(DIST_ZIP)" .)
	@echo "built $(DIST_ZIP) ($$(stat -c '%s' $(DIST_ZIP)) bytes)"

# --- Music render tool: host-native, not part of the DOS build -------------
#
# Re-renders gameSource/music/SONG.WAV from the unmodified synthesis logic
# (musicPlayer.cpp's synthesizeAudioCallback, Timbre.cpp, Envelope.cpp) --
# only needed if the song, timbres, or envelopes ever change. Not run by
# `make all`/`make game`; builds its own host-native static SDL3 once into
# $(BUILD_DIR)/host-sdl3 (separate from the DJGPP cross-build's sysroot).

HOST_SDL3_DIR := $(BUILD_DIR)/host-sdl3
RENDER_MUSIC_SRC := $(REPO_ROOT)/tools/render-music/render_music.cpp

# DJGPP_BIN/DJGPP_TBIN are prepended onto PATH (and exported) above by
# sdl3-dos.mk for the cross-build -- host-native cmake/gcc invocations
# here must NOT see them (a DJGPP cross `as` doesn't understand a host
# x86_64 `as --64` invocation and fails cryptically). HOST_PATH strips
# them back out; HOST_CXX is the system compiler, not $(CXX) (which
# sdl3-dos.mk points at the DJGPP cross g++).
HOST_PATH := /usr/local/bin:/usr/bin:/bin
HOST_CXX  := g++

$(HOST_SDL3_DIR)/libSDL3.a:
	mkdir -p $(HOST_SDL3_DIR)
	PATH=$(HOST_PATH) cmake -S $(VENDOR_DIR)/SDL -B $(HOST_SDL3_DIR) \
	    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=OFF
	PATH=$(HOST_PATH) $(MAKE) -C $(HOST_SDL3_DIR) -j$$(nproc) SDL3-static

.PHONY: render-music
render-music: $(HOST_SDL3_DIR)/libSDL3.a
	PATH=$(HOST_PATH) $(HOST_CXX) -std=c++14 -O2 \
	    -I$(VENDOR_DIR)/SDL/include -I$(VENDOR_DIR) -I$(PASSAGE_SRC) \
	    $(AUDIO_TIER_FLAGS) \
	    $(RENDER_MUSIC_SRC) \
	    $(PASSAGE_SRC)/musicPlayer.cpp \
	    $(PASSAGE_SRC)/Timbre.cpp \
	    $(PASSAGE_SRC)/Envelope.cpp \
	    $(PASSAGE_SRC)/common.cpp \
	    $(MINORGEMS_SRC)/io/file/linux/PathLinux.cpp \
	    $(MINORGEMS_SRC)/system/unix/TimeUnix.cpp \
	    $(MINORGEMS_SRC)/system/linux/ThreadLinux.cpp \
	    $(MINORGEMS_SRC)/util/stringUtils.cpp \
	    -L$(HOST_SDL3_DIR) -lSDL3 -lpthread -lm \
	    -o $(BUILD_DIR)/render_music
	cd $(PASSAGE_SRC) && $(BUILD_DIR)/render_music music/SONG.WAV
