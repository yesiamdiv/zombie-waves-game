#ifndef CAMERA_H
#define CAMERA_H

#include "core/mathutil.h"

typedef struct {
    Vec2 position;   /* world position of camera center */
    float zoom;
    float target_zoom;
    float smoothing;
    int viewport_w;
    int viewport_h;
} Camera;

void camera_init(Camera *cam, int viewport_w, int viewport_h);
void camera_follow(Camera *cam, Vec2 target, float dt);
void camera_set_position(Camera *cam, Vec2 pos);
void camera_apply_zoom(Camera *cam, float dt);
/* Keep the camera center inside the world so the view never pans past the
 * border walls into the void (B6). No-op when the viewport exceeds the world. */
void camera_clamp_world(Camera *cam, float world_w, float world_h);

/* Convert between world and screen space */
Vec2 camera_world_to_screen(Camera *cam, Vec2 world_pos);
Vec2 camera_screen_to_world(Camera *cam, Vec2 screen_pos);

/* Check if a world-space rectangle is visible */
bool camera_is_visible(Camera *cam, Vec2 pos, float margin);

#endif
