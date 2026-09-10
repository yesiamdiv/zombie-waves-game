#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "ecs/ecs.h"
#include "core/input.h"
#include "world/world.h"
#include "world/camera.h"
#include <SDL3/SDL.h>

/* Movement system - applies velocity to positions, respects world bounds */
void system_movement(World *ecs, GameWorld *world, float dt);

/* Collision system - entity vs entity and entity vs world */
void system_collision(World *ecs, GameWorld *world);

/* Player input system - reads input and moves player, handles shooting */
void system_player_input(World *ecs, InputState *input, Camera *cam, float dt);

/* Zombie AI system - chase, attack behavior */
void system_zombie_ai(World *ecs, float dt);

/* Bullet system - moves bullets, checks lifetime, deals damage */
void system_bullets(World *ecs, GameWorld *world, float dt);

/* Particle system - updates and renders particles */
void system_particles(World *ecs, float dt);

/* Render system - draws all visible sprites */
void system_render(World *ecs, SDL_Renderer *renderer, Camera *cam);

/* Animation system */
void system_animation(World *ecs, float dt);

/* Cleanup dead entities */
void system_cleanup(World *ecs);

#endif
