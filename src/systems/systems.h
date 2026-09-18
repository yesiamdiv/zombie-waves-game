#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "ecs/ecs.h"
#include "core/input.h"
#include "world/world.h"
#include "world/camera.h"
#include "world/waves.h"
#include "weapons/weapons.h"
#include "players.h"
#include "net/net_mirror.h"
#include <SDL3/SDL.h>

/* Movement system - applies velocity to positions, respects world bounds */
void system_movement(World *ecs, GameWorld *world, float dt);

/* Collision system - entity vs entity and entity vs world */
void system_collision(World *ecs, GameWorld *world);

/* Player input system - reads `p->input`, moves `p->entity`, handles shooting.
 * Runs once per resident player slot. */
void system_player_input(World *ecs, Player *p, Camera *cam, float dt);

/* Zombie AI system - chase, attack behavior. Zombies chase the NEAREST alive
 * player (slot table when provided; raw player-entity scan otherwise) and
 * damage exactly that target. */
void system_zombie_ai(World *ecs, Player *players, int player_count, float dt);

/* Bullet system - moves bullets, checks lifetime, deals damage */
void system_bullets(World *ecs, GameWorld *world, float dt);

/* Weapon systems - sword spin (held click), grenades (AoE on detonation),
 * launcher rockets (piercing, destroyed outside the world). Each operates on
 * one player slot: input is `p->input`, inventory is `p->inventory`, and the
 * player entity is `p->entity`. */
void system_sword(World *ecs, Player *p, float dt);
void system_grenades(World *ecs, Player *p, float dt);
void system_rockets(World *ecs, Player *p, GameWorld *world, float dt);

/* Particle system - updates and renders particles */
void system_particles(World *ecs, float dt);

/* Render system - draws all visible sprites */
void system_render(World *ecs, SDL_Renderer *renderer, Camera *cam);

/* Render-only net client: draw the interpolated snapshot mirror. `render_time`
 * is the local render clock used to blend the mirror's snapshot pair. */
void system_render_mirror(SDL_Renderer *renderer, Camera *cam,
                          const NetMirror *mirror, float render_time);

/* Multiplayer-only: draw the color-coded spawn beacons for every in-use
 * player slot at `slot->beacon_pos` (the respawn anchor in TDM). */
void system_render_beacons(SDL_Renderer *renderer, Camera *cam,
                           const Player *players, int player_count);

/* Animation system */
void system_animation(World *ecs, float dt);

/* Cleanup dead entities (health <= 0). Emits kill events and notifies
 * the wave system of zombie deaths so the kill counter and wave
 * completion logic stay in sync. Zombie kills are credited to the player slot
 * that last damaged them (`players`, nullable when no credit should happen):
 * the bullet/sword/grenade/rocket owner is recorded on the zombie and resolved
 * back through the slot table. */
void system_cleanup(World *ecs, WaveSystem *waves, Player *players, int player_count);

#endif
