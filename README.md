# tintin-nx

Unofficial Nintendo Switch port of **The Adventures of Tintin: The Game HD**
(Gameloft, Android 1.0.5, armeabi-v7a). It runs the game's original 32-bit ARM
libraries on Horizon OS through the
[android32](https://github.com/aks796/android32) runtime.

**No game files are included.** You need your own copy of the game's APK.
This project is not affiliated with or endorsed by Gameloft, Ubisoft,
Moulinsart or Nintendo.

> Work in progress.

## Status

| Feature | State |
| --- | --- |
| Boot, loading, 3D gameplay | working ✓ |
| Touchscreen | working ✓ |
| Audio (Vox through AudioTrack on audout) | working ✓ |
| Gamepad: cursor (right stick / D-pad), A, + | working ✓ |
| Gamepad: left stick to the on-screen joystick | working ✓ |
| Intro and cutscene videos | skipped (the game is told they finished) |
| Online features | not implemented |

## What you need

- A Switch running custom firmware with the homebrew menu
- The game APK: `com.gameloft.android.GAND.GloftTTSS` 1.0.5 (versionCode 105),
  with armeabi-v7a libraries. Put it in `/switch/tintin_nx/`. The runtime's
  setup finds it by its contents and unpacks the libraries on first launch
  (see `runtime/docs/notes/G_setup.md`).

## Building

Requirements (see the android32 README for details):

- Docker and the toolchain image `ghcr.io/vita2hos/devcontainer/vita2hos`
- [libnx32](https://github.com/aks796/libnx32) 4.12.0 or newer, in `..\libnx32\prefix`
- Mesa in `portlibs32/` ([mesa32](https://github.com/aks796/mesa32) or
  [mesa-switch32](https://github.com/aks796/mesa-switch32))
- devkitPro `devkita64` for the launcher NRO

Steps:

```powershell
git submodule update --init   # fetches runtime/ (android32)
.\build.ps1                 # builds the 32-bit program (Docker)
cd launcher; ./build.sh     # wraps it into tintin_nx.nro
```

Copy `launcher/tintin_nx.nro` to `/switch/tintin_nx/` on the SD card.

## Controls

- Touchscreen works as on Android.
- Right stick or D-pad moves a yellow cursor; **A** taps at the cursor.
- With no cursor on screen, **A** presses the main action button.
- **+** is pause.
- Left stick drives the on-screen joystick (in progress).
- **HOME** exits.

Button positions (1280x720) can be changed without rebuilding. Copy
`buttons.example.txt` to `/switch/tintin_nx/buttons.txt` and edit it. Each line
is `NAME x y`; names are `JOY A PLUS B X Y L R ZL ZR MINUS`; a negative `x`
disables that button.

## Credits

- [aks796/android32](https://github.com/aks796/android32) runtime (MIT), and
  the loader work it builds on (TheOfficialFloW, fgsfds, xerpi/vita2hos)
- libnx and the switchbrew authors; Mesa and its Switch backends

## License

The port code is MIT (see `LICENSE`). The game and its assets belong to their
owners and are not part of this repository.

