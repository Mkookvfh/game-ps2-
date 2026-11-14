# game‑ps2‑ (New‑Game‑Assassin‑s‑By‑Clauss)
> Fan‑made Assassin’s Creed prototype (Logic test build for Termux / GitHub Actions, future port to PS2)

## ⚠️ Important Notice
This is a fan‑made project for personal development and testing only. **Not for sale or commercial distribution.** Not affiliated with Ubisoft.

## 🎮 Features (Prototype)
✅ Player state system (Idle / Walk / Run / Crouch / Climb / Look around)
✅ Silent‑kill finishers
‑ Finisher 1: Back silent kill
‑ Finisher 2: Back fast kill
‑ Finisher 3: Side kill
‑ Finisher 4: Grass hidden kill
✅ Guard NPC alert system (0 = Calm / 1 = Suspicious / 2 = Chase)

## 🛠️ Build
### Local (Termux Android)
```bash
pkg update
pkg upgrade
pkg install gcc make git
git clone https://github.com/Mkookvfh/game‑ps2‑.git
cd game‑ps2‑
make
./game_test
