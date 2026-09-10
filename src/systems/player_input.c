#include "systems/systems.h"
#include "world/waves.h"
#include "events/event_bus.h"
#include "core/log.h"

#define PLAYER_SPEED 200.0f
#define BULLET_SPEED 400.0f
#define BULLET_LIFETIME 2.0f
#define SHOOT_COOLDOWN 0.2f

static float shoot_timer = 0;

void system_player_input(World *ecs, InputState *input, Camera *cam, float dt) {
    Entity player = ECS_NULL_ENTITY;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) {
            player = i;
            break;
        }
    }
    if (player == ECS_NULL_ENTITY) return;

    CPosition *pos = ecs_get_position(ecs, player);
    CVelocity *vel = ecs_get_velocity(ecs, player);

    /* Movement */
    Vec2 move_dir = {0, 0};
    if (input_key_held(input, SDL_SCANCODE_W) || input_key_held(input, SDL_SCANCODE_UP))
        move_dir.y -= 1.0f;
    if (input_key_held(input, SDL_SCANCODE_S) || input_key_held(input, SDL_SCANCODE_DOWN))
        move_dir.y += 1.0f;
    if (input_key_held(input, SDL_SCANCODE_A) || input_key_held(input, SDL_SCANCODE_LEFT))
        move_dir.x -= 1.0f;
    if (input_key_held(input, SDL_SCANCODE_D) || input_key_held(input, SDL_SCANCODE_RIGHT))
        move_dir.x += 1.0f;

    if (vec2_length(move_dir) > 0.001f) {
        move_dir = vec2_normalize(move_dir);
    }

    vel->vel = vec2_scale(move_dir, PLAYER_SPEED);

    /* Mouse world position for camera and aiming */
    Vec2 mouse_screen = vec2(input->mouse_x, input->mouse_y);
    Vec2 mouse_world = camera_screen_to_world(cam, mouse_screen);
    input->mouse_world_x = mouse_world.x;
    input->mouse_world_y = mouse_world.y;

    /* Shooting: fire while the button is held (human hold-to-fire and
     * AI-driven) at the fire-rate cooldown. */
    shoot_timer -= dt;
    if (input->mouse_buttons[0] && shoot_timer <= 0) {
        shoot_timer = SHOOT_COOLDOWN;

        Vec2 dir = vec2_normalize(vec2_sub(mouse_world, pos->pos));
        if (vec2_length(dir) < 0.001f) dir = vec2(1, 0);

        Entity bullet = ecs_create_entity(ecs);
        if (bullet != ECS_NULL_ENTITY) {
            ecs_add_component(ecs, bullet, COMP_POSITION);
            ecs_add_component(ecs, bullet, COMP_VELOCITY);
            ecs_add_component(ecs, bullet, COMP_SPRITE);
            ecs_add_component(ecs, bullet, COMP_COLLIDER);
            ecs_add_component(ecs, bullet, COMP_BULLET_TAG);

            Vec2 spawn = vec2_add(pos->pos, vec2_scale(dir, 18.0f));
            *ecs_get_position(ecs, bullet) = (CPosition){{spawn.x, spawn.y}};
            *ecs_get_velocity(ecs, bullet) = (CVelocity){
                .vel = vec2_scale(dir, BULLET_SPEED),
                .max_speed = BULLET_SPEED
            };
            *ecs_get_collider(ecs, bullet) = (CCollider){4.0f, true};
            *ecs_get_bullet_tag(ecs, bullet) = (CBulletTag){
                .lifetime = BULLET_LIFETIME,
                .max_lifetime = BULLET_LIFETIME,
                .owner = player
            };

            SDL_FColor bullet_color = {1.0f, 0.9f, 0.3f, 1.0f};
            *ecs_get_sprite(ecs, bullet) = (CSprite){
                .sprite = sprite_circle(4.0f, bullet_color),
                .scale = 1.0f,
                .base_alpha = 1.0f
            };

            event_emit(g_events, GE_PLAYER_SHOT, bullet, GEK_BULLET,
                       spawn.x, spawn.y, dir.x, dir.y, 0, 0);
        }
    }
}
