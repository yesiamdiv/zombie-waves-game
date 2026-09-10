# Open World Zombie Waves

A top-down open-world zombie survival game built with SDL3 in C.

Survive endless waves of zombies in a procedurally generated world with buildings, roads, and ponds.

## Features

- **Wave System**: Escalating difficulty with increasing zombie count and speed
- **Open World**: Tile-based world with walls, roads, water, and buildings
- **ECS Architecture**: Clean Entity Component System for easy extension
- **Sprite Abstraction**: Colored shapes now, texture-ready for later
- **Full HUD**: Health, wave info, kill count, score
- **Menus**: Main menu, pause, game over with stats
- **Items**: Health, ammo, and speed boost pickups
- **Logging**: Full debug logging to file + console
- **Particle Effects**: Death particles for zombies

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Requires: CMake 3.20+, GCC/Clang, SDL3 (fetched automatically via CMake).

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrows | Move |
| Mouse | Aim |
| Left Click | Shoot |
| ESC | Pause |

## Running

```bash
./build/open_world_zombie_waves
```

Logs are written to `game.log` in the working directory.
