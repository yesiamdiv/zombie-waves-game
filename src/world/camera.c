#include "world/camera.h"
#include "core/log.h"

void camera_init(Camera *cam, int viewport_w, int viewport_h) {
    cam->position = vec2(0, 0);
    cam->zoom = 1.0f;
    cam->target_zoom = 1.0f;
    cam->smoothing = 5.0f;
    cam->viewport_w = viewport_w;
    cam->viewport_h = viewport_h;
    LOG_INFO("Camera initialized (%dx%d)", viewport_w, viewport_h);
}

void camera_follow(Camera *cam, Vec2 target, float dt) {
    cam->position.x = lerp(cam->position.x, target.x, cam->smoothing * dt);
    cam->position.y = lerp(cam->position.y, target.y, cam->smoothing * dt);
}

void camera_set_position(Camera *cam, Vec2 pos) {
    cam->position = pos;
}

void camera_apply_zoom(Camera *cam, float dt) {
    cam->zoom = lerp(cam->zoom, cam->target_zoom, 3.0f * dt);
}

Vec2 camera_world_to_screen(Camera *cam, Vec2 world_pos) {
    Vec2 offset = vec2_sub(world_pos, cam->position);
    offset = vec2_scale(offset, cam->zoom);
    return vec2_add(offset, vec2(cam->viewport_w * 0.5f, cam->viewport_h * 0.5f));
}

Vec2 camera_screen_to_world(Camera *cam, Vec2 screen_pos) {
    Vec2 offset = vec2_sub(screen_pos, vec2(cam->viewport_w * 0.5f, cam->viewport_h * 0.5f));
    offset = vec2_scale(offset, 1.0f / cam->zoom);
    return vec2_add(offset, cam->position);
}

bool camera_is_visible(Camera *cam, Vec2 pos, float margin) {
    float half_w = (cam->viewport_w * 0.5f / cam->zoom) + margin;
    float half_h = (cam->viewport_h * 0.5f / cam->zoom) + margin;

    float dx = fabsf(pos.x - cam->position.x);
    float dy = fabsf(pos.y - cam->position.y);

    return dx < half_w && dy < half_h;
}
