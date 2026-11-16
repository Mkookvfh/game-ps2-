MUEANG ISARA: Open World

An Open-World Action Adventure Game for Android

เกมแอ็กชันผจญภัยโลกเปิดที่ได้รับแรงบันดาลใจจากเกมแนว Action-Adventure ยุค PlayStation 2 ผสมผสานระบบ Parkour, Stealth, Combat และ Exploration เข้าด้วยกัน โดยมีเป้าหมายในการสร้างประสบการณ์การเล่นแบบ Open World บนโทรศัพท์ Android

---

🎮 About the Game

MUEANG ISARA: Open World เป็นเกมผจญภัยโลกเปิดที่ผู้เล่นจะได้สำรวจดินแดนโบราณ ป่าเขตร้อน ปราสาท และสถานที่ลึกลับ พร้อมใช้ทักษะการเคลื่อนไหว การลอบเร้น และการต่อสู้เพื่อเอาชีวิตรอด

โปรเจกต์นี้พัฒนาด้วยภาษา C และ Android NDK โดยใช้ OpenGL ES 2.0 สำหรับการเรนเดอร์กราฟิก มีการออกแบบโครงสร้าง Shared Game Logic เพื่อรองรับการพัฒนาระบบเกมร่วมกันระหว่างแพลตฟอร์มในอนาคต

Key Features

- Open World Exploration
- Third-Person Character Controller
- Advanced Parkour System
- Stealth and Assassination
- Dynamic NPC and Guard AI
- 3D Character and Skeleton Animation
- Grappling Hook and Zipline
- Environmental Interaction
- Android Touch Controls

---

⚔️ Gameplay Systems

1. Movement & Parkour

ระบบเคลื่อนไหวและปีนป่ายประกอบด้วย

- Walking and Running
- Sprint
- Jumping
- Rolling
- Sliding
- Diving
- Wall Running
- Wall Jump
- Pole Vault
- Leap of Faith
- Safe Landing

2. Traversal System

ระบบเดินทางและเคลื่อนที่ผ่านสภาพแวดล้อม

- Grappling Hook
- Zipline
- Wall Traversal
- Pole Vault
- Environmental Climbing
- Air Descent

3. Stealth System

ระบบลอบเร้นและกำจัดศัตรู

- Crouching
- Stealth Movement
- Crowd Blending
- Air Assassination
- Ledge Takedown
- Enemy Detection
- Guard Alert System

4. Combat & NPC AI

ระบบต่อสู้และ AI

- NPC Patrol
- Enemy Awareness
- Guard Detection
- Enemy Attack Behavior
- Enemy Defeat Tracking

5. Character & Animation

ระบบตัวละคร 3D

- 3D Character Model
- 21-Bone Skeleton Hierarchy
- Procedural Character Animation
- Character Movement States
- Cape Secondary Motion
- GLB Model Loading
- Humanoid Animation Pipeline

Animation Standard: Hybrid Mixamo + Unreal Engine 5 Compatibility

หมายเหตุ: ระบบโครงกระดูกและ Animation Pipeline อยู่ระหว่างการพัฒนาและเชื่อมต่อกับการเล่นจริงเพิ่มเติม

6. Exploration & Immersion

ระบบสำรวจโลก

- Synchronization Points
- Ancient Sight
- Environmental Clues
- Hidden Locations
- Nature-Based Navigation

7. Resource Harvesting & Crafting

ระบบที่วางแผนพัฒนาต่อ

- Collecting Herbs
- Gathering Wood
- Mining Minerals
- Resource Inventory
- Tool Upgrades
- Backpack Upgrades
- Crafting
- Potion Creation
- Resource Respawn

---

🌍 World Design

โลกของ MUEANG ISARA มีเป้าหมายในการสร้างพื้นที่สำรวจที่มีเอกลักษณ์และบรรยากาศแบบดินแดนโบราณ

สถานที่ที่วางแผนไว้ ได้แก่

- Hidden Waterfall
- Ancient Lost Temple
- Sky Cliffs
- Emerald River
- Misty Bamboo Forest
- Sacred Flower Valley

สถานที่เหล่านี้จะรองรับการสำรวจ การปีนป่าย การค้นหาความลับ และกิจกรรมภายในโลกเกม

---

🛠️ Technology Stack

Technology| Purpose
C| Core Game Logic
Android NDK| Native Android Development
OpenGL ES 2.0| 3D Rendering
CMake| Native Build System
GLB / glTF| 3D Model Format
Skeleton System| Character Rigging
Mixamo| Animation Pipeline
GitHub Actions| Automated Android Build

Project Architecture

game-ps2-/
│
├── shared/
│   ├── game.h
│   └── game.c
│
├── platform/
│   └── android/
│       └── app/
│           └── src/
│               └── main/
│                   ├── cpp/
│                   │   ├── native_main.c
│                   │   ├── renderer3d.c
│                   │   ├── character3d.c
│                   │   ├── glb_loader.c
│                   │   ├── skeleton.c
│                   │   ├── animation.c
│                   │   └── CMakeLists.txt
│                   │
│                   └── assets/
│                       └── models/
│
└── README.md

---

📱 Android Development

เกมพัฒนาสำหรับ Android โดยใช้ Native C/C++ Toolchain ผ่าน Android NDK และ CMake

Build Process

โปรเจกต์ใช้ GitHub Actions สำหรับการ Build APK อัตโนมัติ

1. Push หรือ Commit โค้ดลง Branch ที่กำหนด
2. GitHub Actions เริ่มกระบวนการ Build
3. ตรวจสอบผลการ Build
4. ดาวน์โหลด APK จาก Workflow Artifacts
5. ติดตั้ง APK บนอุปกรณ์ Android เพื่อทดสอบ

Development Branch

"feature/android-ps2-open-world"

Repository

https://github.com/Mkookvfh/game-ps2-

---

🗺️ Development Roadmap

System| Status
Android Native Foundation| Implemented
OpenGL ES 2.0 Renderer| Implemented
Third-Person Movement| Implemented
Parkour System| Implemented
Stealth System| Implemented
Grappling Hook| Implemented
Zipline| Implemented
NPC Guard AI| Implemented
Skeleton Hierarchy| Initial Implementation
GLB Character Pipeline| In Progress
Full Skeletal Skinning| Planned
Animation Playback Integration| Planned
Resource Harvesting| Planned
Inventory System| Planned
Crafting System| Planned
Ancient Sight| Planned
Body Hiding System| Planned
Nemesis Rival System| Planned
Expanded Open World| Planned

สถานะข้างต้นเป็นภาพรวมตามการพัฒนาในปัจจุบัน ไม่ใช่ผลการทดสอบล่าสุดของทุกระบบ

---

🎯 Development Goals

เป้าหมายหลักของโปรเจกต์คือ

1. สร้างเกม Open World ที่เล่นได้จริงบน Android
2. พัฒนาระบบ Parkour และ Stealth ให้สมบูรณ์
3. เพิ่มระบบ Animation และ Skeletal Skinning แบบเต็มรูปแบบ
4. เพิ่มระบบเก็บทรัพยากรและ Crafting
5. สร้างโลกที่มีสถานที่ลับและกิจกรรมให้ค้นพบ
6. ปรับปรุงประสิทธิภาพให้เหมาะกับโทรศัพท์ Android
7. ออกแบบ Shared Game Logic เพื่อรองรับการ Port ไปยังแพลตฟอร์มอื่นในอนาคต

---

📌 Project Status

MUEANG ISARA: Open World is an ongoing development project.

โปรเจกต์นี้ยังอยู่ระหว่างการพัฒนา ระบบและเนื้อหาบางส่วนยังไม่สมบูรณ์ และจะมีการปรับปรุงเพิ่มเติมอย่างต่อเนื่อง

---

👨‍💻 Development

Developed as an independent game development project.

Project: MUEANG ISARA: Open World
Platform: Android
Engine Technology: Custom C / OpenGL ES 2.0
Build System: Android NDK + CMake
Repository: Mkookvfh/game-ps2-