#include "events/event_bus.h"
#include "core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EventBus *g_events = NULL;

static const char *event_type_names[GE_COUNT] = {
    [GE_NONE]            = "NONE",
    [GE_PLAYER_SHOT]     = "PLAYER_SHOT",
    [GE_DAMAGE]          = "DAMAGE",
    [GE_KILL]            = "KILL",
    [GE_ENTITY_SPAWN]    = "ENTITY_SPAWN",
    [GE_ENTITY_DEATH]    = "ENTITY_DEATH",
    [GE_WAVE_START]      = "WAVE_START",
    [GE_WAVE_END]        = "WAVE_END",
    [GE_PLAYER_HEALTH]   = "PLAYER_HEALTH",
    [GE_ITEM_PICKUP]     = "ITEM_PICKUP",
    [GE_POSITION_SAMPLE] = "POSITION_SAMPLE",
    [GE_INPUT]           = "INPUT",
    [GE_POINTS]          = "POINTS",
    [GE_SHOP_PURCHASE]   = "SHOP_PURCHASE",
    [GE_HOST_PAUSE]      = "HOST_PAUSE",
};

static const char *event_kind_names[GEK_COUNT] = {
    [GEK_NONE]     = "-",
    [GEK_PLAYER]   = "player",
    [GEK_ZOMBIE]   = "zombie",
    [GEK_BULLET]   = "bullet",
    [GEK_SWORD]    = "sword",
    [GEK_GRENADE]  = "grenade",
    [GEK_ROCKET]   = "rocket",
    [GEK_ITEM]     = "item",
    [GEK_PARTICLE] = "particle",
};

EventBus *event_bus_init(const char *path) {
    if (g_events) {
        LOG_WARN("Event bus already initialized; re-initializing");
        event_bus_shutdown(g_events);
    }

    EventBus *bus = (EventBus *)calloc(1, sizeof(EventBus));
    if (!bus) {
        LOG_ERROR("Failed to allocate event bus");
        return NULL;
    }

    bus->pos_sample_interval = 0.2f;  /* 5 samples/sec */
    bus->pos_sample_timer = 0.0f;
    bus->start_wall_time = SDL_GetTicks() / 1000.0;

    if (path) {
        bus->file = fopen(path, "w");
        if (!bus->file) {
            LOG_ERROR("Failed to open event log '%s'", path);
        } else {
            fprintf(bus->file, "# open-world-zombie-waves gameplay event log\n");
            fprintf(bus->file, "# format: t=<seconds> f=<frame> sid=<serial> EVT=<type> e=<entity> k=<kind> [payload]\n");
            fprintf(bus->file, "# see docs/EVENT_FORMAT.md\n");
        }
    } else {
        LOG_DEBUG("Event bus running without a log file (memory only)");
    }

    g_events = bus;
    LOG_INFO("Event bus initialized (events file: %s)", path ? path : "(none)");
    return bus;
}

void event_bus_shutdown(EventBus *bus) {
    if (!bus) return;
    if (bus->file) {
        event_bus_flush(bus);
        fclose(bus->file);
    }
    if (g_events == bus) g_events = NULL;
    free(bus);
}

void event_bus_push(EventBus *bus, const GameEvent *event) {
    if (!bus) return;
    int slot = (bus->head + bus->count) % EV_MAX_EVENTS;
    bus->ring[slot] = *event;

    if (bus->count < EV_MAX_EVENTS) {
        bus->count++;
    } else {
        /* Ring full: drop oldest (head advances) */
        bus->head = (bus->head + 1) % EV_MAX_EVENTS;
        LOG_WARN("Event bus ring full; dropping oldest event");
    }
}

void event_emit(EventBus *bus, GameEventType type, Entity entity, EventKind kind,
                float x, float y, float a, float b, int ia, int ib) {
    if (!bus) return;
    GameEvent ev = {
        .type = type,
        .frame = bus->frame,
        .time_sec = bus->total_time,
        .entity = entity,
        .kind = kind,
        .x = x, .y = y,
        .a = a, .b = b,
        .ia = ia, .ib = ib,
        .sid = bus->next_sid++
    };
    event_bus_push(bus, &ev);
}

void event_bus_flush(EventBus *bus) {
    if (!bus || !bus->file) return;

    while (bus->count > 0) {
        const GameEvent *ev = &bus->ring[bus->head];
        const char *tn = ev->type < GE_COUNT ? event_type_names[ev->type] : "UNKNOWN";
        const char *kn = ev->kind < GEK_COUNT ? event_kind_names[ev->kind] : "-";

        fprintf(bus->file, "t=%.3f f=%llu sid=%llu EVT=%s e=%u k=%s",
                ev->time_sec, (unsigned long long)ev->frame,
                (unsigned long long)ev->sid, tn, ev->entity, kn);

        if (ev->x != 0.0f || ev->y != 0.0f)
            fprintf(bus->file, " x=%.1f y=%.1f", ev->x, ev->y);
        if (ev->a != 0.0f || ev->b != 0.0f)
            fprintf(bus->file, " a=%.2f b=%.2f", ev->a, ev->b);
        if (ev->ia != 0 || ev->ib != 0)
            fprintf(bus->file, " ia=%d ib=%d", ev->ia, ev->ib);

        fputc('\n', bus->file);

        bus->head = (bus->head + 1) % EV_MAX_EVENTS;
        bus->count--;
    }
    fflush(bus->file);
}

void event_bus_tick(EventBus *bus, double dt) {
    if (!bus) return;
    bus->frame++;
    bus->total_time += dt;
    bus->pos_sample_timer += (float)dt;
}

int event_bus_samples(EventBus *bus, void *ecs_ptr, float view_w, float view_h) {
    (void)view_w;
    (void)view_h;
    if (!bus || !ecs_ptr) return 0;

    if (bus->pos_sample_timer < bus->pos_sample_interval) return 0;
    bus->pos_sample_timer = 0;

    World *ecs = (World *)ecs_ptr;
    int count = 0;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;

        Vec2 pos = ecs->positions[i].pos;
        EventKind kind = GEK_NONE;

        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) {
            kind = GEK_PLAYER;
        } else if (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG)) {
            kind = GEK_ZOMBIE;
        } else if (ecs->component_masks[i] & (1u << COMP_BULLET_TAG)) {
            kind = GEK_BULLET;
        } else if (ecs->component_masks[i] & (1u << COMP_GRENADE_TAG)) {
            kind = GEK_GRENADE;
        } else if (ecs->component_masks[i] & (1u << COMP_ROCKET_TAG)) {
            kind = GEK_ROCKET;
        } else if (ecs->component_masks[i] & (1u << COMP_ITEM_TAG)) {
            kind = GEK_ITEM;
        }

        if (kind != GEK_NONE) {
            event_emit(bus, GE_POSITION_SAMPLE, i, kind, pos.x, pos.y,
                       0, 0, 0, 0);
            count++;
        }
    }
    return count;
}