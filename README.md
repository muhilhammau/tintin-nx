<div align="center">

<img src="launcher/icon.jpg" alt="The Adventures of Tintin: The Game HD" width="180">

# tintin_nx

**The Adventures of Tintin: The Game HD on Nintendo Switch**

An unofficial Nintendo Switch wrapper/port of Gameloft's Android game.

[![Switch](https://img.shields.io/badge/Nintendo_Switch-Homebrew-E60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)](#installation)
[![Version](https://img.shields.io/badge/version-v1.0.0-4C8BF5?style=for-the-badge)](#)
[![Android](https://img.shields.io/badge/Android-1.0.5-3DDC84?style=for-the-badge&logo=android&logoColor=white)](#about)
[![AArch32](https://img.shields.io/badge/AArch32-32--bit-0091BD?style=for-the-badge&logo=arm&logoColor=white)](#building)

</div>

---

## About

`tintin_nx` runs the original 32-bit Android version of **The Adventures of Tintin: The Game HD** on Nintendo Switch, using the open-source [android32](https://github.com/aks796/android32) runtime.

The supported game build is:

`text
Package:      com.gameloft.android.GAND.GloftTTSS
Version:      1.0.5
Version code: 105
ABI:          armeabi-v7a / AArch32
The original ARM game library runs directly inside a 32-bit Horizon OS process. Players must provide their own APK and game data files (gameloft/games/GloftTTSS/); these are not included in the repository or release download.Status: Boot, 3D gameplay, touchscreen, OpenSL ES / audio out, and gamepad joystick/cursor controls are functional.FeaturesOriginal Android 1.0.5 game running natively in AArch32 on Switch.1280 × 720 widescreen presentation with touchscreen and gamepad integration.Dynamic cursor navigation for touch-oriented menus (Right Stick / D-Pad + A button).Direct game SD data mounting (gameloft/games/GloftTTSS/).Native checkpoint saves and settings persistence.Audio playback via android32 AudioTrack / OpenSL ES driver.64-bit NRO launcher carrying the 32-bit payload with sphaira HOME-menu forwarder support.InstallationRequirementsA Nintendo Switch running Atmosphère custom firmware.sphaira for installing the HOME-menu forwarder.Your own supported 1.0.5 ARMv7 APK (com.gameloft.android.GAND.GloftTTSS).Extracted Gameloft SD data folder (GloftTTSS).Copy the switch/ folder to the root of your SD card. Ensure the game files are placed according to the layout below:Plaintextsd:/switch/tintin_nx/
├── tintin_nx.nro
├── The-Adventures-of-Tintin.apk
├── gameloft/
│   └── games/
│       └── GloftTTSS/
│           ├── anims/
│           ├── collisions/
│           ├── datalibs/
│           ├── levels/
│           ├── models/
│           ├── music/
│           ├── shaders/
│           ├── text/
│           ├── textures/
│           ├── videos/
│           ├── oconf.bar
│           ├── sound.pack
│           └── states
First launchOpen sphaira and locate The Adventures of Tintin in Homebrew.Select Install Forwarder.Launch the game icon from the Switch HOME menu.On initial launch, the runtime extracts necessary native libraries from the APK and initializes local configurations.ControlsGameplayNintendo SwitchInputLeft StickCharacter movement / On-screen joystickRight Stick / D-PadMove virtual cursorATap cursor position / Primary actionB / X / YContext action buttons+Pause / Game menuTouchscreenNative touch controlsCursor / NavigationMove the Right Stick or D-Pad to reveal the yellow selection cursor.Press A to execute a touch tap at the cursor's location.Button positions can be modified by creating buttons.txt inside /switch/tintin_nx/ (refer to buttons.example.txt).File Structure & SavesThe application directory is located under sd:/switch/tintin_nx/:Plaintextdata/              Android internal storage and native saves
gameloft/          Gameloft shared data (gameloft/games/GloftTTSS/)
sdcard/            Android external storage root
buttons.txt        Custom button mapping overrides
debug.log          Current execution log
crash.log          Crash diagnostics output
BuildingRequirements:Docker with Linux container support.android32 in runtime/ (Git submodule).libnx32 prefix (version 4.12.0 or newer).mesa32 or mesa-switch32 in portlibs32/.devkitPro devkita64 for launcher compilation.Build steps:PowerShellgit submodule update --init --recursive
.\build.ps1
cd launcher; ./build.sh
The compiled binary will be located at launcher/tintin_nx.nro.Creditsaks796/android32 — core AArch32 Android runtime.libnx32 and Mesa Switch contributors — SDK and graphics stack.tasm2_nx — documentation and structure reference.Gameloft, Ubisoft, and Moulinsart — original game developers and rights holders.LicenseThe port code is licensed under MIT (see LICENSE). Game assets and binaries remain the property of their respective owners.
