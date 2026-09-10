# Extending Open World Zombie Waves

## Project Structure

```
src/
├── main.c              Entry point, game state machine, reset logic
├── core/
│   ├── log.h/c         Logging system with levels + assertions
│   ├── input.h/c       Keyboard and mouse input
│   └── mathutil.h      Vec2 math, collision helpers, lerp, clamp
├── ecs/
│   └── ecs.h/c         Entity Component System (bitmask-based)
├── graphics/
│   └── sprite.h/c      Sprite abstraction (shapes -> textures)
├── world/
│   ├── camera.h/c      Camera with smooth follow + zoom
│   ├── world.h/c       Tile-based world map
│   └── waves.h/c       Wave spawning system
├── systems/
│   ├── systems.h       All system declarations
│   ├── movement.c      Applies velocity, world collision
│   ├── collision.c     Entity-vs-entity collision (bullets, zombies)
│   ├── player_input.c  Player movement + shooting
│   ├── zombie_ai.c     Chase / attack AI
│   ├── bullets.c       Bullet lifetime + bounds
│   ├── particles.c     Particle update + fade
│   └── render.c        Draws all sprites + health bars
├── ui/
│   ├── menu.h/c        Main menu, pause, game over screens
│   └── hud.h/c         In-game HUD (health, wave, kills)
└── items/
    └── items.h/c       Health, ammo, speed pickups
```

There are no per-entity files: entities are built from components by **factory
functions** (e.g. `create_player` in `main.c`, `waves_spawn_zombie` in
`waves.c`, `items_spawn` in `items.c`). Add a new entity by writing a new
factory function that assembles the right components.

## How to Add a New Component

1. Add a new `COMP_*` enum value in `src/ecs/ecs.h`
2. Define the component struct in `ecs.h`
3. Add a storage array in the `World` struct
4. Add an accessor function (`ecs_get_*`)
5. Add initialization in the entity creation code

Example: Adding a `CShield` component:

```c
// In ecs.h enum:
COMP_SHIELD,

// In ecs.h struct:
typedef struct {
    float amount;
    float regen_rate;
} CShield;

// In World struct:
CShield shields[ECS_MAX_ENTITIES];

// Accessor:
static inline CShield *ecs_get_shield(World *w, Entity e) {
    return &w->shields[ecs_get_entity_index(w, e)];
}
```

## How to Add a New Entity Type

1. Write a factory function that:
   - Calls `ecs_create_entity()`
   - Adds required components via `ecs_add_component()`
   - Sets initial component values
2. Call the factory from `main.c`, a wave spawner, or an item loader

## How to Add a New System

1. Add declaration in `src/systems/systems.h`
2. Implement in a new `.c` file under `src/systems/`
3. Call it in `main.c`'s `update()` function at the right point in the pipeline

System execution order matters:
```
player_input -> zombie_ai -> movement -> collision -> bullets -> particles -> cleanup
```

## How to Swap Shapes for Textures

The `Sprite` struct uses a `SpriteShape` enum. Currently supports:
- `SPRITE_SHAPE_RECT` - Colored rectangle
- `SPRITE_SHAPE_CIRCLE` - Colored circle
- `SPRITE_SHAPE_TEXTURE` - (reserved for SDL_Texture)

To add texture support:
1. Add `SDL_Texture *texture` and `SDL_FRect src_rect` to the `Sprite` union
2. In `sprite_draw()`, handle `SPRITE_SHAPE_TEXTURE` with `SDL_RenderTexture()`
3. Load textures in a resource manager and assign to sprites

## Logging

Use the log macros for all debug output:

```c
LOG_TRACE("Very verbose");      // Development only
LOG_DEBUG("Debug info");        // Development only
LOG_INFO("Important events");   // Always visible
LOG_WARN("Something odd");      // Warning
LOG_ERROR("Something failed");  // Error
LOG_FATAL("Critical failure");  // Aborts

ASSERT(ptr != NULL);
ASSERT_MSG(count > 0, "count was %d", count);
```

Logs go to both console and `game.log` file.

## Adding New Item Types

1. Add enum value in `ItemType` (src/items/items.h)
2. Handle in `items_spawn()` switch
3. Handle pickup effect in `items_check_pickup()` switch

## Adding New Zombie Types

Extend `CZombieTag` with variant-specific fields, then vary stats in `waves_spawn_zombie()`.
