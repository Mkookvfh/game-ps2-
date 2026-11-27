# Tropical Island Adventure — PS2 Prototype Plan

## Goal

Turn this repository into an original single-player PS2-era tropical-island action-adventure prototype inspired by the *feel* of Far Cry 3: exploration, dense island scenery, hostile outposts, wildlife, and mission-driven progression.

This is **not** a Far Cry 3 port or a recreation of Ubisoft's copyrighted story, characters, maps, or assets. The working title is **Island Frontier**. Use original characters, locations, missions, and low-poly assets.

## PS2-friendly scope

The PS2 has limited memory and fill rate, so the prototype should prioritize a small, playable island slice rather than an enormous seamless AAA world.

- One compact island region with a beach, jungle trail, village, and one outpost.
- Third-person player movement and a simple follow camera.
- A small number of visible NPCs and wildlife at a time.
- Low-poly geometry, vertex colors, simple textures, fog, and distance-based object visibility.
- Fixed-step gameplay update and a stable target frame rate.
- No assumption that modern shaders, large texture sets, or complex physics are available.

## First playable milestone

The first milestone is a **walkable island test scene**, not a complete game:

1. A bright sky and blue sea surrounding a sandy shore.
2. A green jungle path leading inland.
3. A small original wooden hut and lookout structure.
4. A player that can move, turn, jump, and collide with the ground and simple obstacles.
5. A follow camera that stays behind the player and avoids extreme angles.
6. A simple HUD showing a health bar and a mission prompt.
7. A clear return-to-start area so the test can be repeated.

## Original island layout

Use a compact test map divided into readable zones:

- **Palm Beach** — safe starting area, shoreline, rocks, and palms.
- **Green Trail** — winding path with trees and simple cover.
- **Fisher Village** — a few huts and a small dock.
- **Lookout Camp** — a small hostile outpost used for the first mission.
- **Hill Shrine** — a visible landmark that helps the player orient themselves.

Keep the first version small and use placeholder geometry before adding detailed models.

## Gameplay systems, in order

### Phase 1 — Playable foundation
- Re-enable and verify the PS2 scene draw calls.
- Player movement, camera, ground collision, and map boundaries.
- Island terrain, sea, beach, and basic foliage.
- HUD and one objective marker.

### Phase 2 — First mission
- Mission states: not started, active, completed, failed.
- Objective: reach the lookout camp and interact with a marked object.
- Mission text and completion feedback.
- Restart/reset the test mission.

### Phase 3 — Basic living world
- One friendly NPC with simple idle/walk behavior.
- One patrol NPC with a small waypoint loop.
- Simple detection radius and alert state.
- A small wildlife actor that wanders within a bounded area.

### Phase 4 — Combat prototype
- A simple original close-range or ranged interaction.
- Health, damage, cooldown, and defeat states.
- Keep combat non-graphic and use simple placeholder effects.
- Avoid adding complex enemy behavior until movement and collision are stable.

### Phase 5 — Save and polish
- Save player position and mission state.
- Pause menu and controller instructions.
- Distance-based draw/visibility rules.
- Performance testing on real PS2 hardware or a compatible emulator.

## Controls (initial proposal)

- Left analog stick / D-pad: move.
- Right analog stick: camera.
- Cross: jump / confirm.
- Circle: interact / cancel.
- Square: basic action.
- R1: run.
- Start: pause.

Adjust the mapping after the controller input code is verified.

## Technical guardrails

- Keep the native Termux logic test separate from the PS2 graphics executable.
- Do not describe a native Termux build as a PS2 build.
- Make one small change per milestone and test it before adding another system.
- Avoid enabling multiple large render functions at once; restore them one at a time to identify rendering or performance issues.
- Do not claim a feature is working until it has been built and tested on PS2SDK or a compatible emulator.

## Immediate next coding task

After the build instructions in PR #2 are reviewed, the next code task should be to create a clean **island scene module** (terrain, sea, beach, palms, and huts) and connect it to the main render loop behind a simple scene switch. Keep the existing city prototype available until the island scene is verified.
