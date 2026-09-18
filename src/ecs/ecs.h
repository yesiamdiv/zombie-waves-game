#ifndef ECS_H
#define ECS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "core/log.h"
#include "core/mathutil.h"
#include "graphics/sprite.h"

#define ECS_MAX_ENTITIES 2048
#define ECS_MAX_COMPONENTS 32

typedef uint32_t Entity;
#define ECS_NULL_ENTITY UINT32_MAX

typedef uint32_t ComponentType;

/* Component IDs - add new components here */
enum {
    COMP_POSITION = 0,
    COMP_VELOCITY,
    COMP_HEALTH,
    COMP_SPRITE,
    COMP_COLLIDER,
    COMP_PLAYER_TAG,
    COMP_ZOMBIE_TAG,
    COMP_BULLET_TAG,
    COMP_SWORD_TAG,
    COMP_GRENADE_TAG,
    COMP_ROCKET_TAG,
    COMP_ITEM_TAG,
    COMP_ANIMATION,
    COMP_PARTICLE,
    COMP_COUNT
};

/* Component data structures */
typedef struct {
    Vec2 pos;
} CPosition;

typedef struct {
    Vec2 vel;
    float max_speed;
} CVelocity;

typedef struct {
    float current;
    float max;
} CHealth;

typedef struct {
    Sprite sprite;
    float scale;
    float base_alpha;
} CSprite;

typedef struct {
    float radius;
    bool is_trigger;
} CCollider;

typedef struct {
    int empty;
} CPlayerTag;

typedef enum {
    ZOMBIE_IDLE,
    ZOMBIE_CHASE,
    ZOMBIE_ATTACK,
    ZOMBIE_HURT,
    ZOMBIE_DEAD
} ZombieState;

typedef struct {
    ZombieState state;
    float attack_timer;
    float attack_cooldown;
    float detection_range;
    float attack_range;
    float hurt_timer;
    /* Entity that last damaged this zombie (a player). Used by system_cleanup
     * to credit the correct player's inventory for the kill. */
    Entity last_hit_by;
} CZombieTag;

typedef struct {
    float lifetime;
    float max_lifetime;
    Entity owner;
} CBulletTag;

/* Spinning sword: orbits the owner while the fire button is held. */
typedef struct {
    Entity owner;
    float radius;        /* orbit radius around the owner */
    float angle;         /* current angle (radians) */
    float spin_speed;    /* radians / second */
    float damage;
    float hit_timer;     /* time until the next hit is allowed */
    float hit_interval;
} CSwordTag;

/* Grenade: flies toward the aim point, then explodes in an area of effect. */
typedef struct {
    Entity owner;
    float fuse;              /* seconds until detonation */
    float max_fuse;
    float explosion_radius;
    float damage;
} CGrenadeTag;

/* Launcher rocket: flies straight, damages every zombie it passes through,
 * and is destroyed once it leaves the world. */
typedef struct {
    Entity owner;
    float damage;
    float lifetime;
} CRocketTag;

typedef enum {
    ITEM_HEALTH,
    ITEM_AMMO,
    ITEM_SPEED_BOOST,
    ITEM_COUNT
} ItemType;

typedef struct {
    ItemType type;
    float value;
    float bob_timer;
} CItemTag;

typedef struct {
    int current_frame;
    float frame_timer;
    float frame_duration;
    int total_frames;
} CAnimation;

typedef struct {
    float lifetime;
    float max_lifetime;
    Vec2 vel;
    float size_decay;
} CParticle;

/* ECS World */
typedef struct {
    Entity entities[ECS_MAX_ENTITIES];
    bool alive[ECS_MAX_ENTITIES];
    uint32_t component_masks[ECS_MAX_ENTITIES];
    uint32_t entity_count;
    uint32_t alive_count;

    /* Component storage - arrays indexed by entity index */
    CPosition  positions[ECS_MAX_ENTITIES];
    CVelocity  velocities[ECS_MAX_ENTITIES];
    CHealth    healths[ECS_MAX_ENTITIES];
    CSprite    sprites[ECS_MAX_ENTITIES];
    CCollider  colliders[ECS_MAX_ENTITIES];
    CPlayerTag player_tags[ECS_MAX_ENTITIES];
    CZombieTag zombie_tags[ECS_MAX_ENTITIES];
    CBulletTag bullet_tags[ECS_MAX_ENTITIES];
    CSwordTag  sword_tags[ECS_MAX_ENTITIES];
    CGrenadeTag grenade_tags[ECS_MAX_ENTITIES];
    CRocketTag rocket_tags[ECS_MAX_ENTITIES];
    CItemTag   item_tags[ECS_MAX_ENTITIES];
    CAnimation animations[ECS_MAX_ENTITIES];
    CParticle  particles[ECS_MAX_ENTITIES];
} World;

void ecs_init(World *world);
Entity ecs_create_entity(World *world);
void ecs_destroy_entity(World *world, Entity entity);
bool ecs_is_alive(World *world, Entity entity);
void ecs_add_component(World *world, Entity entity, ComponentType type);
void ecs_remove_component(World *world, Entity entity, ComponentType type);
bool ecs_has_component(World *world, Entity entity, ComponentType type);
uint32_t ecs_get_entity_index(World *world, Entity entity);

/* Convenience component accessors */
static inline CPosition *ecs_get_position(World *w, Entity e) {
    return &w->positions[ecs_get_entity_index(w, e)];
}
static inline CVelocity *ecs_get_velocity(World *w, Entity e) {
    return &w->velocities[ecs_get_entity_index(w, e)];
}
static inline CHealth *ecs_get_health(World *w, Entity e) {
    return &w->healths[ecs_get_entity_index(w, e)];
}
static inline CSprite *ecs_get_sprite(World *w, Entity e) {
    return &w->sprites[ecs_get_entity_index(w, e)];
}
static inline CCollider *ecs_get_collider(World *w, Entity e) {
    return &w->colliders[ecs_get_entity_index(w, e)];
}
static inline CPlayerTag *ecs_get_player_tag(World *w, Entity e) {
    return &w->player_tags[ecs_get_entity_index(w, e)];
}
static inline CZombieTag *ecs_get_zombie_tag(World *w, Entity e) {
    return &w->zombie_tags[ecs_get_entity_index(w, e)];
}
static inline CBulletTag *ecs_get_bullet_tag(World *w, Entity e) {
    return &w->bullet_tags[ecs_get_entity_index(w, e)];
}
static inline CSwordTag *ecs_get_sword_tag(World *w, Entity e) {
    return &w->sword_tags[ecs_get_entity_index(w, e)];
}
static inline CGrenadeTag *ecs_get_grenade_tag(World *w, Entity e) {
    return &w->grenade_tags[ecs_get_entity_index(w, e)];
}
static inline CRocketTag *ecs_get_rocket_tag(World *w, Entity e) {
    return &w->rocket_tags[ecs_get_entity_index(w, e)];
}
static inline CItemTag *ecs_get_item_tag(World *w, Entity e) {
    return &w->item_tags[ecs_get_entity_index(w, e)];
}
static inline CParticle *ecs_get_particle(World *w, Entity e) {
    return &w->particles[ecs_get_entity_index(w, e)];
}

#endif
