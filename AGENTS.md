# AGENTS.md

Notes for coding agents working on this repository (a reimplementation of
Super Mario World, with ports for desktop and Nintendo Switch).

This file only documents things that are not obvious from reading the source.
Keep it up to date when you learn something new.

## Environment

* The Switch toolchain (devkitPro/devkitA64, libnx, switch-sdl2) is installed
  system-wide, but **not on `PATH` by default**. Run this in every shell before
  building:

  ```sh
  source /etc/profile.d/devkit-env.sh
  ```

* Desktop Linux has SDL2 development files; `pkg-config --exists sdl2` succeeds.
* There is **no Switch hardware and no Switch emulator** on the development
  machine. Switch runtime behaviour can only be verified by the user. Never
  claim a runtime fix is confirmed when only a build was verified.
* Asset extraction needs an unheadered USA ROM as `smw.sfc` in the repository
  root. `smw_assets.dat` is generated from it by `assets/restool.py`.

## Build commands

Switch (from `src/platform/switch`):

```sh
source /etc/profile.d/devkit-env.sh
make clean
make -j$(nproc)
```

Outputs in `src/platform/switch/`: `smw.nro`, `smw.nacp`, `smw.elf`.
`make clean` removes `bin/`, `romfs/`, `smw.nro`, `smw.nacp`, `smw.elf`.

Desktop syntax check without touching the desktop build state:

```sh
gcc -fsyntax-only -std=gnu99 -I. -Isrc -Ithird_party $(pkg-config --cflags sdl2) src/main.c src/config.c
```

## Switch build details

* `src/platform/switch/Makefile` adds the `smb1/` and `smbll/` directories to
  `SOURCES`. Without them linking fails with undefined references to
  `SmasSpcPlayer_Create`, `kSmb1GameInfo` and `kSmbllGameInfo`.
* The Makefile builds a RomFS (`ROMFS := romfs`) containing `smw.ini` (copied
  from `src/platform/switch/smw.ini`) and `smw_assets.dat`. Both are embedded
  into the `.nro`, so nothing has to be copied to the SD card next to it.
  If `smw_assets.dat` is missing it is regenerated with `assets/restool.py`.
* `PYTHON ?= python3` so Windows/MSYS2 users can pass `make PYTHON=python`.
* Do **not** use a global `VPATH`. devkitPro's `switch_rules` uses `VPATH` to
  locate generated object files, so stale x86-64 `.o` files from a desktop
  build in `src/`, `src/snes/` or `third_party/gl_core/` are treated as
  up-to-date and the link fails with "cannot find <file>.o". The Makefile uses
  `SOURCES_ABS` plus pattern-specific `vpath %.c/%.s/%.cpp` instead, which only
  searches for source files.
* Verify a build with:
  * `echo $?` is 0 and the log contains no `error:` / `undefined reference`
  * `smw.nro` exists and starts with `HOMEBREW` at offset 8 and `NRO0` at 16
  * `Writing .../romfs/smw.ini to RomFS image...` and
    `Writing .../romfs/smw_assets.dat to RomFS image...` appear in the log
* Recent devkitPro uses a hashed RomFS format, so do not expect an `IVFC`
  magic in the NRO. To verify the contents, search the file for the asset
  signature (`Smw_v0` prefix in `assets/smw_assets.h`) and for the
  `[General]` header of `smw.ini`.

## Switch runtime gotchas (already fixed, do not regress)

* **Launch arguments are not a ROM path.** `main()` used to do
  `argc--, argv++` and then load `argv[0]` as a SNES ROM. On Switch, hbmenu
  (applet mode) and forwarders pass the `.nro` path as an argument, so the game
  loaded its own NRO as a ROM and crashed Atmosphere. On `__SWITCH__` all
  launch arguments are now ignored and the ROM comes from the embedded assets.
* **Never call `SDL_ShowSimpleMessageBox` before `SDL_Init` on Switch.** It is
  not implemented and can crash the console. `Die()` prints to the console
  screen (`PrintErrorMessageToScreen`) only before SDL is initialized, and
  otherwise just logs and exits. `Warning()` never shows a message box on
  Switch.
* `appletInitializeGamePlayRecording()` allocates a 96 MB TransferMemory in
  application mode. It is intentionally not called: it adds startup memory
  pressure and is unusable in applet mode anyway.
* Working directory: libnx sets the cwd from `argv[0]` only when *not* running
  as an NSO, so hbmenu launches already have the NRO directory as cwd, but
  forwarders do not. `SwitchImpl_Init` falls back to
  `sdmc:/switch/smw` when `envIsNso()` is true or the cwd is not writable.
  Saves and save states are written relative to the cwd (`saves/`).
* Config resolution order: `smw.user.ini` → `smw.ini` (cwd) →
  `romfs:/smw.ini`. Asset resolution order on Switch: `romfs:/smw_assets.dat`
  → `smw_assets.dat` → `assets/smw_assets.dat`.
* `romfsInit()` must be called before any asset/config load and `romfsExit()`
  before exit; both happen in `SwitchImpl_Init` / `SwitchImpl_Exit`.
* SDL's Switch video driver already calls `appletMainLoop()` from
  `SWITCH_PumpEvents`, so the normal SDL loop handles applet exit. Any
  standalone error loop must still poll `appletMainLoop()`.
* Renderer: the accelerated (GLES2) renderer can be unavailable; the SDL
  renderer falls back to software. `SDL_LockTexture` failures must never hand
  a `NULL` pixel buffer to the PPU drawing code.
* Audio: on Switch a failed `SDL_OpenAudioDevice` is retried at 48000 Hz, and
  if that also fails the game runs without sound while `RtlRenderAudio` is
  pumped once per frame from the main loop, because the game waits on APU
  state (e.g. `APUI02`) that is otherwise only advanced by the audio callback.

## Files worth knowing

* `src/main.c` — entry point, SDL setup, renderer/audio, asset loading,
  argument handling.
* `src/config.c` — ini parsing, key/gamepad mapping, config file fallbacks.
* `src/platform/switch/Makefile` — Switch build and RomFS generation.
* `src/platform/switch/src/switch_impl.c` — libnx init/exit, romfs, cwd,
  console error screen.
* `assets/restool.py` — generates `smw_assets.dat` (includes the ROM, so the
  game no longer needs `smw.sfc` at runtime).
* `BUILDING.md` — user-facing instructions, including the Switch section.

## Repository hygiene

* `smw_assets.dat`, `smw.sfc`, `saves/`, `bin/`, `romfs/`, `*.nro`, `*.nacp`
  and `*.elf` are git-ignored. Do not commit build outputs or the ROM.
* Do not modify `third_party/` unless the task requires it.
* Match the style of surrounding code (2-space indent, snake_case for
  reimplementation symbols, CamelCase for platform/SDL helpers).
