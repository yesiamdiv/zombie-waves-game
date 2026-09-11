#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "ecs/ecs.h"
#include "core/mathutil.h"

#define EV_MAX_EVENTS 8192

typedef enum {
    GE_NONE = 0,
    GE_PLAYER_SHOT,
    GE_DAMAGE,
    GE_KILL,
    GE_ENTITY_SPAWN,
    GE_ENTITY_DEATH,
    GE_WAVE_START,
    GE_WAVE_END,
    GE_PLAYER_HEALTH,
    GE_ITEM_PICKUP,
    GE_POSITION_SAMPLE,
    GE_INPUT,
    GE_POINTS,
    GE_SHOP_PURCHASE,
    GE_COUNT
} GameEventType;

typedef enum {
    GEK_NONE = 0,
    GEK_PLAYER,
    GEK_ZOMBIE,
    GEK_BULLET,
    GEK_SWORD,
    GEK_GRENADE,
    GEK_ROCKET,
    GEK_ITEM,
    GEK_PARTICLE,
    GEK_COUNT
} EventKind;

typedef struct {
    GameEventType type;
    Uint64 frame;
    double time_sec;
    Entity entity;
    EventKind kind;   /* primary participant */
    float x, y;       /* position (world) */
    float a, b;       /* generic float payloads */
    int ia, ib;       /* generic int payloads */
    Uint64 sid;       /* monotonic serial, unique per event line */
} GameEvent;

typedef struct {
    GameEvent ring[EV_MAX_EVENTS];
    int head;
    int count;
    Uint64 frame;
    double total_time;
    FILE *file;
    float pos_sample_interval;
    float pos_sample_timer;
    double start_wall_time;
    Uint64 next_sid;
} EventBus;

/* The event bus is a process-global singleton. It is write-only during
 * gameplay; the recorder flushes buffered events to disk per frame. */
extern EventBus *g_events;

/* Returns NULL on failure (logging already emitted). */
EventBus *event_bus_init(const char *path);
void event_bus_shutdown(EventBus *bus);

/* Push a single event into memory (no I/O). */
void event_bus_push(EventBus *bus, const GameEvent *event);

/* Convenience emitter with a fully-formed event. */
void event_emit(EventBus *bus, GameEventType type, Entity entity, EventKind kind,
                float x, float y, float a, float b, int ia, int ib);

/* Flush buffered events to the log file. Called once per game frame. */
void event_bus_flush(EventBus *bus);

/* Advance the frame counter / total time once per game frame. */
void event_bus_tick(EventBus *bus, double dt);

/* Periodically emit GE_POSITION_SAMPLE entries for important entities.
 * Returns number of samples emitted. */
int event_bus_samples(EventBus *bus, void *ecs, float view_w, float view_h);

#endif