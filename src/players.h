#ifndef PLAYERS_H
#define PLAYERS_H

#include "ecs/ecs.h"
#include "core/input.h"
#include "graphics/sprite.h"
#include "weapons/weapons.h"
#include "game_mode.h"
#include <stdbool.h>

#define MAX_PLAYERS 4

/* One player slot. Single-player keeps exactly one slot in use; multiplayer
 * fills up to MAX_PLAYERS. Systems receive a `Player *` so input, inventory,
 * and kill credit all resolve against the correct slot - the per-player
 * tables replace the old single `InputState` + `PlayerInventory` globals.
 *
 * `input` is "posted":
 *   - local slot: copied from the real SDL input state each frame,
 *   - remote slot (multiplayer): written by the net layer,
 *   - AI/bot slot: written through the ai_apply_controls() injection path.
 * Systems only ever read the displayed slot, never global input directly. */
typedef struct Player {
    bool in_use;
    bool alive;          /* false while dead (respawn pending or eliminated) */
    bool eliminated;     /* hardcore mode: dead for the rest of the match */
    char name[32];
    SDL_FColor color;    /* sprite + beacon (P0.5) + HUD color */
    Entity entity;       /* owned player entity; ECS_NULL_ENTITY when none */
    InputState input;    /* posted input for this slot */
    PlayerInventory inventory;
    int kills;           /* personal kill count */
    float shoot_cd;      /* per-player pistol cooldown */
    float respawn_timer; /* TDM: seconds until respawn while dead (<=0 idle) */
    Vec2 beacon_pos;     /* player-colored spawn beacon anchor (P0.5): the
                            visible, color-coded marker at this player's spawn
                            point; also used as the respawn anchor in TDM */
} Player;

/* Zero every slot (does not touch the ECS). */
void players_reset(Player *players, int count);

/* Spawn a fresh player entity for slot `idx` at `pos`. Resets the slot's
 * input, inventory, and alive/eliminated state. Returns the slot index, or -1
 * if the slot is invalid/already in use or the ECS is out of entities.
 * `slot_name`/`color` label the player (beacon/HUD/net purposes). */
int player_respawn(Player *players, World *ecs, int idx, const char *slot_name,
                   const SDL_FColor *color, Vec2 pos);

/* Index of the in-use slot owning `entity`, else -1. */
int players_find_index(const Player *players, int count, Entity entity);

/* TDM respawn: recreate `idx`'s entity at their own beacon, preserving the
 * slot's inventory/name/color/kills. Returns idx or -1 (invalid slot / no ECS
 * space). */
int player_respawn_slot(Player *players, World *ecs, int idx);

/* per-frame death/respawn rule update (call AFTER system_cleanup). See the
 * .c for the SINGLE / TDM / HARDCORE semantics. Returns alive slot count. */
int players_match_update(World *ecs, Player *players, int count, GameMode mode,
                         float dt);

#endif