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
