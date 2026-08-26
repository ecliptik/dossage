```yaml
name: dossage
upstream: https://github.com/jasonrohrer/Passage
upstream_revision: 2f713f261dc907f6feda106ebbee0464eeab791d
upstream_license: public domain
language: C
original_sdl_version: "1.2"
target_sdl_version: "3"
graphics_api: software
native_resolution: 640x480 (default; settings/screenWidth.ini + screenHeight.ini)
color_depth: TBD (host build uses SDL 1.2 default surface format; recorded once ported)
audio_stack: raw SDL_OpenAudio callback, from-scratch software synth (Timbre/Envelope) -- no SDL_mixer file decode
input_stack: keyboard (arrow keys); SDL_INIT_JOYSTICK requested unconditionally on non-Mac, unused otherwise
filesystem_dependencies: settings/*.ini (read/write), gameSource/*.tga graphics, music/music.tga
threading: none (only Thread::staticSleep() -- a sleep, not a spawned thread)
networking: none
external_dependencies: minorGems (subset -- file/path, string, settings, time, thread, sha1, TGA image decode, simple vector)
copyrighted_assets: none -- engine and assets both public domain, ship together
dos_status: BOOTSTRAP
dos_minimum_target: 486DX2-50
dos_recommended_target: 486DX2-50
maintainer: ecliptik
```

`dos_status` values (same ladder as `ports.yaml` in the sdl-dos-ports hub):

```
BACKLOG
RESEARCH
BLOCKED
BOOTSTRAP
COMPILES
STARTS
TITLE_SCREEN
PLAYABLE
FULL_GAME
OPTIMIZING
RELEASE_READY
```

Difficulty score (also tracked in the hub's `ports.yaml`): **1 (trivial)**.
