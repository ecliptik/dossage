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

.PHONY: all
all: sdl3 game

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

GAME_CXXFLAGS := \
    -I$(VENDOR_DIR) \
    -I$(SYSROOT)/include \
    -march=i486 -mtune=pentium -O2 -fno-rtti \
    $(NOSIMD_FLAGS)

%.o: %.cpp
	$(CXX) $(GAME_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/dossage.exe: $(SYSROOT)/lib/libSDL3.a $(GAME_OBJECTS)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(GAME_CXXFLAGS) -o $@ $(GAME_OBJECTS) -L$(SYSROOT)/lib -lSDL3 -lm
	$(STUBEDIT) $@ minstack=2048k

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

.PHONY: stage
stage: $(BUILD_DIR)/dossage.exe
	@test -f "$(CWSDPMI_EXE)" || (echo "error: $(CWSDPMI_EXE) missing -- run ./scripts/fetch-vendor-binaries.sh" >&2; exit 1)
	mkdir -p "$(STAGE_DIR)"
	install -m 0644 $(BUILD_DIR)/dossage.exe "$(STAGE_DIR)/DOSSAGE.EXE"
	install -m 0644 $(CWSDPMI_EXE)           "$(STAGE_DIR)/CWSDPMI.EXE"
	rm -rf "$(STAGE_DIR)/graphics" "$(STAGE_DIR)/music" "$(STAGE_DIR)/settings"
	cp -r $(PASSAGE_SRC)/graphics "$(STAGE_DIR)/graphics"
	cp -r $(PASSAGE_SRC)/music    "$(STAGE_DIR)/music"
	cp -r $(PASSAGE_SRC)/settings "$(STAGE_DIR)/settings"
