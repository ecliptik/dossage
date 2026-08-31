# tests/probes/probes.mk -- build rules for this port's own standalone
# DJGPP diagnostic probes (tests/probes/*.c).
#
# Deliberately decoupled from the game build and from sdl3-dos.mk's SDL3
# cross-build stage: these probes are pure DJGPP libc + DPMI, no SDL, no
# engine, no C++ (per the probe-engineer charter's "Build via a dedicated
# probes target, separate from the main game build"). A probe target here
# never depends on $(SYSROOT)/lib/libSDL3.a.
#
# Included from the top-level Makefile, which by this point has already
# included .sdl-dos-ports/shared/build/sdl3-dos.mk -- so REPO_ROOT,
# BUILD_DIR, CC (i586-pc-msdosdjgpp-gcc) and STUBEDIT are already defined
# and PATH already carries the DJGPP cross-toolchain.
#
# Usage:
#   make probes            # build every probe below
#   make probe-dlygran     # build just DLYGRAN.EXE
#   make probe-clkscale    # build just CLKSCALE.EXE
#   make probes-clean

PROBES_DIR       := $(REPO_ROOT)/tests/probes
PROBES_BUILD_DIR := $(BUILD_DIR)/probes

PROBE_CFLAGS := -march=i486 -mtune=pentium -O2 -Wall -Wextra

# --- Probe registry: name -> source file + extra link libs -----------------
# Binary/log names are each probe's own 8.3 DOS name (see its header
# comment); the build here always names the .exe after the DOS binary
# name, not the source basename, so `install`/stage steps don't need a
# rename.

PROBE_dlygran_SRC  := $(PROBES_DIR)/dlygran.c
PROBE_dlygran_EXE  := $(PROBES_BUILD_DIR)/DLYGRAN.EXE
PROBE_dlygran_LIBS := -lm

PROBE_clkdrift_SRC  := $(PROBES_DIR)/clkdrift.c
PROBE_clkdrift_EXE  := $(PROBES_BUILD_DIR)/CLKDRIFT.EXE
PROBE_clkdrift_LIBS := -lm

PROBE_clkscale_SRC  := $(PROBES_DIR)/clkscale.c
PROBE_clkscale_EXE  := $(PROBES_BUILD_DIR)/CLKSCALE.EXE
PROBE_clkscale_LIBS := -lm

PROBES := dlygran clkdrift clkscale

$(PROBES_BUILD_DIR):
	mkdir -p $(PROBES_BUILD_DIR)

define PROBE_RULE
.PHONY: probe-$(1)
probe-$(1): $$(PROBE_$(1)_EXE)

$$(PROBE_$(1)_EXE): $$(PROBE_$(1)_SRC) | $(PROBES_BUILD_DIR)
	$$(CC) $$(PROBE_CFLAGS) -o $$@ $$< $$(PROBE_$(1)_LIBS)
	$$(STUBEDIT) $$@ minstack=2048k
	@# DJGPP's gcc emits a SECOND, lowercased-extension copy of the
	@# executable alongside the -o target (its stubify step normalizes the
	@# extension to .exe). Only the -o target gets the stubedit above, so
	@# on a case-sensitive host the two files diverge: FOO.EXE carries the
	@# intended 2048k minstack, FOO.exe silently keeps DJGPP's 512k
	@# default. Both look plausible -- same size, same timestamp, adjacent
	@# in the directory -- so staging the wrong one is a live foot-gun that
	@# would run the probe on a quarter of the intended stack and be
	@# attributed to anything but the build. The game build never hits this
	@# (its -o target is already lowercase, so stubedit lands on the same
	@# file); it only bites here, where probes use uppercase 8.3 DOS names.
	@# Caught 2026-09-01 when a probe's .EXE/.exe pair diverged and the
	@# operator refused to stage on a hash mismatch. Delete the byproduct
	@# so there is exactly one binary per probe and no ambiguity about
	@# which one was measured.
	rm -f $$(basename $$@).exe
endef

$(foreach p,$(PROBES),$(eval $(call PROBE_RULE,$(p))))

.PHONY: probes
probes: $(foreach p,$(PROBES),probe-$(p))

.PHONY: probes-clean
probes-clean:
	rm -rf $(PROBES_BUILD_DIR)
