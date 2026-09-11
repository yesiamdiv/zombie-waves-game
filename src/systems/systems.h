#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "ecs/ecs.h"
#include "core/input.h"
#include "world/world.h"
#include "world/camera.h"
#include "world/waves.h"
#include "weapons/weapons.h"
#include <SDL3/SDL.h>

/* Movement system - applies velocity to positions, respects world bounds */
void system_movement(World *ecs, GameWorld *world, float dt);

/* Collision system - entity vs entity and entity vs world */
void system_collision(World *ecs, GameWorld *world);

/* Player input system - reads input and moves player, handles shooting */
void system_player_input(World *ecs, InputState *input, Camera *cam, float dt,
                         const PlayerInventory *inv);

/* Zombie AI system - chase, attack behavior */
void system_zombie_ai(World *ecs, float dt);

/* Bullet system - moves bullets, checks lifetime, deals damage */
void system_bullets(World *ecs, GameWorld *world, float dt);

/* Weapon systems - sword spin (held click), grenades (AoE on detonation),
 * launcher rockets (piercing, destroyed outside the world). */
void system_sword(World *ecs, InputState *input, const PlayerInventory *inv, float dt);
void system_grenades(World *ecs, InputState *input, PlayerInventory *inv, float dt);
void system_rockets(World *ecs, InputState *input, PlayerInventory *inv,
                    GameWorld *world, float dt);

/* Particle system - updates and renders particles */
void system_particles(World *ecs, float dt);

/* Render system - draws all visible sprites */
void system_render(World *ecs, SDL_Renderer *renderer, Camera *cam);

/* Animation system */
void system_animation(World *ecs, float dt);

/* Cleanup dead entities (health <= 0). Emits kill events and notifies
 * the wave system of zombie deaths so the kill counter and wave
 * completion logic stay in sync. Awards kill points to `inv` (nullable). */
void system_cleanup(World *ecs, WaveSystem *waves, PlayerInventory *inv);

#endif
