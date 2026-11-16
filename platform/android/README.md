<!-- coderabbit-ai-generated:v1 -->

# Android prototype

## Controls

- Move with the left joystick; drag the right side to look. Jump, sprint and action buttons are on the right. Hold the green resource button near a colored marker to collect it.
- Tap `I` to open inventory. The game pauses while it is open. Resources are `H` herb, `W` wood, `S` stone, `I` iron ore and `F` fiber.
- In inventory, tap `P H2 F1` to make a potion, `T W3 S2 I1` to upgrade the tool, or `B W2 F3` to upgrade the backpack. Green means crafting succeeded; red means ingredients are missing or the limit was reached. Tool and backpack each have two upgrade levels. Tap `USE P` to restore health with a potion.
- Tap `S` to mark living guards; tap `B` near a body to pick it up. Tap `B` again near a tree or hay to hide it, or elsewhere to drop it. Guards react to visible bodies.
- The red-marked guard is the rival. Defeat it with repeated action presses; it can return stronger. `D` shows the number of discovered landmarks.

Inventory, potions, upgrades, rival rank and landmark discoveries are saved in app-private storage every 10 seconds and on pause. Position, current enemies and resource respawn timers reset when the app restarts.

## Build and test

Use JDK 17, Gradle 8.9, Android SDK platform 35, NDK 27.0.12077973 and CMake 3.22.1. Run `sh tests/run_android_tests.sh` from the repository root, then `gradle -p platform/android assembleDebug`. The debug APK is at `platform/android/app/build/outputs/apk/debug/app-debug.apk`.

<!-- coderabbit-ai-generated:v1 -->

<!-- coderabbit-ai-generated-section:v1:778e5016307d14b8ccd56dde3c94b3209a9c3c7cf85f973531739c78f4b11110 -->

## Rigged character and clips

<!-- coderabbit-ai-generated:v1 -->

<!-- coderabbit-ai-generated-section:v1:80417b0270ee8972b655e811852a0a482b1a896d3ac41d6b2d127af460dba968 -->

The game includes `app/src/main/assets/models/MUEANG_ISARA_Rigged_Prototype.glb`, generated from the original character using the existing 21-bone layout and weights. Its ten gameplay clips are generated from the existing procedural poses. Regenerate it from the repository root with `python3 platform/android/tools/generate_rigged_character.py platform/android/app/src/main/assets/models/MUEANG_ISARA_Main_Assassin_02.glb platform/android/app/src/main/assets/models/MUEANG_ISARA_Rigged_Prototype.glb`.

To use a custom character, add a self-contained GLB at `app/src/main/assets/models/MUEANG_ISARA_Rigged.glb` with one skinned mesh and up to 128 joints. Triangle primitives, `JOINTS_0` and `WEIGHTS_0`, and at least one animation are required. Clips named `Idle`, `Walk`, `Jump`, `Roll`, `Slide`, `WallRun`, `Grapple`, `Vault`, `Attack`, and `Crouch` are selected for the corresponding game state; a missing clip falls back to `Idle`, then the bind pose. Names are case insensitive. Clips loop and support STEP or LINEAR translation, rotation, and scale. The current renderer supports base colors but rejects textures, sparse accessors, mesh compression, morph targets, required extensions, additional joint sets and cubic spline animation. If the custom GLB is missing or invalid, the game uses the prototype; if that is unavailable it uses the original character. Invalid models are logged.

<!-- /coderabbit-ai-generated:v1 -->

The GLB parser uses cgltf 1.15 (MIT), vendored from `jkuhlmann/cgltf` tag `v1.15`, commit `360db1a95480fe102ae9c69b27c5d101167ff5ba`. Skin deformation runs on the CPU; performance and appearance need testing on Android devices.

## Remaining work

The bundled rig and clips are generated prototypes. A production character, authored clips, device performance and visual testing remain outstanding. World landmarks and rival behavior are also initial prototypes.

<!-- /coderabbit-ai-generated:v1 -->
