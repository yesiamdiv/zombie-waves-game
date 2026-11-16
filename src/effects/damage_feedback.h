#ifndef DAMAGE_FEEDBACK_H
#define DAMAGE_FEEDBACK_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>

#include "events/event_bus.h"
#include "world/camera.h"

/* E2-E6: the damage-feedback system -- the one place that turns gameplay
 * events into things the player sees.
 *
 * It reads the event bus and nothing else. That is the design decision worth
 * stating, because it is what makes every effect here legal on a net client:
 *
 *   - The host owns every value shown. A damage number is `ev->a` as the host
 *     computed it; a points popup is the host's award. The client never
 *     derives or invents one (docs/SYSTEM_DESIGN_RULES.md -- B19-B23 were all
 *     clients supplying values the host owns).
 *   - Relayed events are re-emitted verbatim into the client's local bus by
 *     net_client.c, so the same consumer works unchanged on both ends. There
 *     is no second code path to keep in sync.
 *   - Trigger sites are not duplicated. audio_tick() walks the same ring for
 *     the same reason, so adding an effect never means editing gameplay code.
 *
 * Like the popup system it consumes, this is cosmetic only: it never writes
 * simulation state and never draws from the global rand() stream. main.c does
 * not tick it in headless mode, so it cannot perturb the single-player
 * byte-identity gate even in principle.
 */

/* Clears effect state and re-arms the event watermark against `bus`, so a new
 * match does not replay feedback for events the previous one emitted. Safe
 * with a NULL bus. */
void damage_feedback_reset(const EventBus *bus);

/* Consumes every event newer than the watermark and ages live effects.
 * `anchor_x/anchor_y` is where feedback for an event that carries no world
 * position goes -- the local player's position. Call once per frame, before
 * event_bus_flush() drains the ring. */
void damage_feedback_tick(const EventBus *bus, float dt,
                          float anchor_x, float anchor_y);

/* World-space effects that are not text popups (E4 muzzle flash). Draw after
 * the entities and before the HUD. */
void damage_feedback_draw(SDL_Renderer *renderer, Camera *cam);

/* E5: how strongly entity `id` should be tinted red right now, 1 at the moment
 * of the hit falling to 0 as the flash expires; 0 for anything not currently
 * hit. Both render paths ask this -- the host's ECS loop and the client's
 * mirror loop -- so the same zombie flashes at the same moment on both
 * screens, driven by the host's relayed GE_DAMAGE and nothing local. */
float damage_feedback_hit_flash(Entity id);

/* Diagnostics for the tests. */
int damage_feedback_events_consumed(void);
int damage_feedback_flash_count(void);

#endif
