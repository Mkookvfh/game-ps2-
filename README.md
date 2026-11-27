# game-ps2- (New-Game-Assassin-s-By-Clauss)

> Fan-made Assassin's Creed-inspired prototype. The repository contains a native PS2 graphics prototype and a separate desktop/Termux logic smoke test.

## Notice
This is a fan-made project for personal development and testing only. It is not affiliated with Ubisoft. Do not sell or commercially distribute Ubisoft-owned content, trademarks, or assets.

## Current prototype
- PS2 3D scene and controller code: `src/scene3d.c`
- World/city/era data: `src/world.c`
- Native gameplay logic test: `src/main.c`, `src/player.c`, `src/ai_npc.c`, `src/combat.c`

This is an early prototype, not a complete Assassin's Creed game. The native logic test does not run the PS2 3D scene.

## Build a native logic test (Android Termux)
This build uses the ordinary Termux compiler and is only for checking the gameplay logic on Android. It does **not** produce a PS2 ELF.

```bash
pkg update
pkg install clang make git
git clone https://github.com/Mkookvfh/game-ps2-.git
cd game-ps2-
make -f Makefile.termux
./game_test
```

To remove the native test binary:

```bash
make -f Makefile.termux clean
```

## Build the PS2 ELF
A PS2 build requires a host environment with PS2SDK and gsKit installed and configured. A regular Android/Termux GCC build cannot compile the PS2 graphics code.

After configuring PS2SDK and gsKit on a supported build host:

```bash
make clean
make
```

The PS2 Makefile is configured to produce `MueangIsara.elf`. This ELF is a homebrew executable for testing; it is not an ISO/disc image. Creating and testing a bootable disc image is a separate packaging step.

## Build files
- `Makefile`: PS2SDK / gsKit build
- `Makefile.termux`: native gameplay logic smoke test
