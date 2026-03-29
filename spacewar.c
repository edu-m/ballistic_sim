#include "font.h"
#include "models.h"
#include "structs.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define OLED 0
#define SOUND 1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TWO_PI M_PI * 2.0f
#define TIME_TO_LOCK 5.0f
#define MAX_LOCK_DIST 5500.0f

#define FPS_TARGET 180

volatile uint32_t audio_t = 0;
volatile int sfx_hmg = 0;
volatile int sfx_exp = 0;
volatile float audio_lock_ratio = 0.0f;
volatile int audio_locked = 0;

Planet scene_planets[2] = {{{12000.0f, 300.0f, 1800.0f}, 1400.0f},
                           {{-10000.0f, -800.0f, 1000.0f}, 850.0f}};

#if SOUND == 0
void audio_callback(void *userdata, Uint8 *stream, int len) { return; }
#else

void audio_callback(void *userdata, Uint8 *stream, int len) {
  for (int i = 0; i < len; i++) {
    uint32_t t = audio_t++;
    int mixed_sample = 0;

    if (sfx_hmg > 0) {
      mixed_sample += ((t * (sfx_hmg >> 3)) ^ (t >> 2)) & 127;
      sfx_hmg -= 1;
      if (sfx_hmg < 0)
        sfx_hmg = 0;
    }

    if (sfx_exp > 0) {
      uint32_t noise = ((t * 214013) + 2531011) >> 16;
      mixed_sample += (noise & 127) * sfx_exp;
      sfx_exp -= 1;
    }

    if (audio_locked) {
      mixed_sample += (t * 12 & 128) ? 64 : 0;
    } else if (audio_lock_ratio > 0.01f) {
      int f_start = 1;
      int f_end = 6;

      float r = audio_lock_ratio;
      float phase = 2.0f * M_PI * TIME_TO_LOCK *
                    (f_start * r + ((f_end - f_start) / 2.0f) * r * r * r);

      int beep_on = sinf(phase - 0.1f) > 0.0f;

      if (beep_on) {
        int pitch = 6;
        mixed_sample += (t * pitch & 128) ? 64 : 0;
      }
    }

    if (mixed_sample > 255)
      mixed_sample = 255;
    stream[i] = (Uint8)mixed_sample;
  }
}
#endif

int width;
int height;
SDL_Renderer *renderer;

Vec2 project_persp(Vec3 v) {
  Vec2 p;
  float z = v.z < 0.001f ? 0.001f : v.z;
  p.x = v.x / z;
  p.y = v.y / z;
  return p;
}

Vec2 screen(Vec2 p) {
  return (Vec2){(p.x + 1.0f) / 2.0f * width,
                (1.0f - (p.y + 1.0f) / 2.0f) * height};
}

Vec3 apply_camera(Vec3 v, Vec3 cam_pos, float cam_yaw, float cam_pitch) {
  v.x -= cam_pos.x;
  v.y -= cam_pos.y;
  v.z -= cam_pos.z;
  float cy = cosf(-cam_yaw), sy = sinf(-cam_yaw);
  float x1 = v.x * cy - v.z * sy;
  float z1 = v.x * sy + v.z * cy;
  float cp = cosf(-cam_pitch), sp = sinf(-cam_pitch);
  float y2 = v.y * cp - z1 * sp;
  float z2 = v.y * sp + z1 * cp;
  return (Vec3){x1, y2, z2};
}

Vec3 rotate_xz(Vec3 v, float angle) {
  float c = cosf(angle), s = sinf(angle);
  return (Vec3){v.x * c - v.z * s, v.y, v.x * s + v.z * c};
}

float dot_product(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 normalize(Vec3 v) {
  float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len == 0.0f)
    return (Vec3){0, 0, 1};
  return (Vec3){v.x / len, v.y / len, v.z / len};
}

void draw_3d_line(Vec3 w_a, Vec3 w_b, Vec3 cam_pos, float cam_yaw,
                  float cam_pitch) {
  Vec3 c_a = apply_camera(w_a, cam_pos, cam_yaw, cam_pitch);
  Vec3 c_b = apply_camera(w_b, cam_pos, cam_yaw, cam_pitch);

  float near_z = 0.1f;
  if (c_a.z < near_z && c_b.z < near_z)
    return;

  if (c_a.z < near_z) {
    float t = (near_z - c_a.z) / (c_b.z - c_a.z);
    c_a.x += t * (c_b.x - c_a.x);
    c_a.y += t * (c_b.y - c_a.y);
    c_a.z = near_z;
  } else if (c_b.z < near_z) {
    float t = (near_z - c_b.z) / (c_a.z - c_b.z);
    c_b.x += t * (c_a.x - c_b.x);
    c_b.y += t * (c_a.y - c_b.y);
    c_b.z = near_z;
  }

  Vec2 p_a = screen(project_persp(c_a));
  Vec2 p_b = screen(project_persp(c_b));
  SDL_RenderDrawLine(renderer, (int)p_a.x, (int)p_a.y, (int)p_b.x, (int)p_b.y);
}

void draw_planet(int p_idx, Vec3 cam_pos, float cam_yaw, float cam_pitch) {
  Vec3 pos = scene_planets[p_idx].pos;
  float radius = scene_planets[p_idx].radius;
  int segments = 128;
  int parallels = 64;
  int meridians = 64;

  for (int i = 1; i < parallels; i++) {
    float v = (float)i / parallels;
    float theta = v * M_PI;
    float y = cosf(theta) * radius;
    float ring_r = sinf(theta) * radius;

    for (int j = 0; j < segments; j++) {
      float a1 = (float)j / segments * 2.0f * M_PI;
      float a2 = (float)(j + 1) / segments * 2.0f * M_PI;

      Vec3 p1 = {pos.x + cosf(a1) * ring_r, pos.y + y,
                 pos.z + sinf(a1) * ring_r};
      Vec3 p2 = {pos.x + cosf(a2) * ring_r, pos.y + y,
                 pos.z + sinf(a2) * ring_r};

      Vec3 n1 = {p1.x - pos.x, p1.y - pos.y, p1.z - pos.z};
      Vec3 v1 = {cam_pos.x - p1.x, cam_pos.y - p1.y, cam_pos.z - p1.z};
      Vec3 n2 = {p2.x - pos.x, p2.y - pos.y, p2.z - pos.z};
      Vec3 v2 = {cam_pos.x - p2.x, cam_pos.y - p2.y, cam_pos.z - p2.z};

      if ((n1.x * v1.x + n1.y * v1.y + n1.z * v1.z) < 0.0f &&
          (n2.x * v2.x + n2.y * v2.y + n2.z * v2.z) < 0.0f)
        continue;

      draw_3d_line(p1, p2, cam_pos, cam_yaw, cam_pitch);
    }
  }

  for (int i = 0; i < meridians; i++) {
    float a = (float)i / meridians * M_PI;
    float sin_a = sinf(a);
    float cos_a = cosf(a);
    for (int j = 0; j < segments; j++) {
      float t1 = (float)j / segments * 2.0f * M_PI;
      float t2 = (float)(j + 1) / segments * 2.0f * M_PI;
      float bx1 = cosf(t1) * radius;
      float by1 = sinf(t1) * radius;
      float bx2 = cosf(t2) * radius;
      float by2 = sinf(t2) * radius;
      Vec3 p1 = {pos.x + (bx1 * cos_a), pos.y + by1, pos.z + (bx1 * sin_a)};
      Vec3 p2 = {pos.x + (bx2 * cos_a), pos.y + by2, pos.z + (bx2 * sin_a)};
      Vec3 n1 = {p1.x - pos.x, p1.y - pos.y, p1.z - pos.z};
      Vec3 v1 = {cam_pos.x - p1.x, cam_pos.y - p1.y, cam_pos.z - p1.z};
      Vec3 n2 = {p2.x - pos.x, p2.y - pos.y, p2.z - pos.z};
      Vec3 v2 = {cam_pos.x - p2.x, cam_pos.y - p2.y, cam_pos.z - p2.z};
      if ((n1.x * v1.x + n1.y * v1.y + n1.z * v1.z) < 0.0f &&
          (n2.x * v2.x + n2.y * v2.y + n2.z * v2.z) < 0.0f)
        continue;
      draw_3d_line(p1, p2, cam_pos, cam_yaw, cam_pitch);
    }
  }

  float ring_radius = radius * 1.6f;
  float tilt = 0.3f;
  for (int i = 0; i < segments; i++) {
    float a1 = (float)i / segments * 2.0f * M_PI;
    float a2 = (float)(i + 1) / segments * 2.0f * M_PI;

    Vec3 p1 = {pos.x + cosf(a1) * ring_radius,
               pos.y + sinf(a1) * ring_radius * tilt,
               pos.z + sinf(a1) * ring_radius};
    Vec3 p2 = {pos.x + cosf(a2) * ring_radius,
               pos.y + sinf(a2) * ring_radius * tilt,
               pos.z + sinf(a2) * ring_radius};

    draw_3d_line(p1, p2, cam_pos, cam_yaw, cam_pitch);
  }
}

int main() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    return 1;

  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = 8000;
  want.format = AUDIO_U8;
  want.channels = 1;
  want.samples = 512;
  want.callback = audio_callback;

  SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_dev > 0) {
    SDL_PauseAudioDevice(audio_dev, 0);
  }

  SDL_DisplayMode DM;
  SDL_GetCurrentDisplayMode(0, &DM);

  width = DM.w;
  height = DM.h;

  SDL_Window *window = SDL_CreateWindow(
      "Space Combat Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      width, height, SDL_WINDOW_FULLSCREEN_DESKTOP);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_SetRelativeMouseMode(SDL_TRUE);
  SDL_GetWindowSize(window, &width, &height);
  int running = 1;
  SDL_Event event;

  Vec3 player_pos = {0.0f, 0.0f, 0.0f};
  Vec3 player_vel = {0.0f, 0.0f, 0.0f};
  float cam_yaw = 0.0f, cam_pitch = 0.0f;
  float mouse_speed = 0.003f;

  int weapon_mode = 0; // 0 = HMG, 1 = AAM
  float aam_cooldown = 0.0f;
  float hmg_speed = 300.0f;
  float aam_speed = 200.0f;

  int current_locking_target = -1;
  int locked_target_idx = -1;
  float lock_timer = 0.0f;
  int lost_target_idx = -1;
  float lost_track_timer = 0.0f;

#define MAX_PROJ 50
#define MAX_TARGETS 50
#define MAX_PARTICLES 300
#define NUM_STARS 500

  Projectile projectiles[MAX_PROJ] = {0};
  Target targets[MAX_TARGETS] = {0};
  Particle particles[MAX_PARTICLES] = {0};
  Vec3 stars[NUM_STARS];

  const float bound_size = 4000.0f;

  for (int i = 0; i < MAX_TARGETS; i++) {
    targets[i].active = 1;
    targets[i].pos = (Vec3){(rand() % 4000 - 2000), (rand() % 4000 - 2000),
                            (rand() % 4000 - 2000)};
    targets[i].vel = (Vec3){((rand() % 60)) - 30.0f, ((rand() % 20)) - 10.0f,
                            ((rand() % 60)) - 30.0f};
  }

  for (int i = 0; i < NUM_STARS; i++) {
    stars[i] = (Vec3){(rand() % 4000 - 2000), (rand() % 4000 - 2000),
                      (rand() % 4000 - 2000)};
  }

  float dt = 1.0f / FPS_TARGET;
  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  float thrust = 1.0f;
  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        running = 0;
      if (event.type == SDL_MOUSEMOTION) {
        cam_yaw -= event.motion.xrel * mouse_speed;
        cam_pitch += event.motion.yrel * mouse_speed;

        if (cam_pitch > M_PI)
          cam_pitch -= TWO_PI;
        if (cam_pitch < -M_PI)
          cam_pitch += TWO_PI;

        if (cam_yaw > M_PI)
          cam_yaw -= TWO_PI;
        if (cam_yaw < -M_PI)
          cam_yaw += TWO_PI;
      }

      if (event.type == SDL_MOUSEWHEEL) {
        weapon_mode = !weapon_mode;
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LSHIFT) {
        thrust *= 10.0f;
      }
      if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LSHIFT) {
        thrust /= 10.0f;
      }

      if (event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_LEFT) {
        if (aam_cooldown > 0.0f && weapon_mode == 1)
          continue;
        if (weapon_mode == 1)
          aam_cooldown = 12.0f;
        for (int i = 0; i < MAX_PROJ; i++) {
          if (!projectiles[i].active) {
            projectiles[i].active = 1;
            projectiles[i].type = weapon_mode;
            projectiles[i].pos = player_pos;
            projectiles[i].ttl = 20.0f * weapon_mode;

            float speed = (weapon_mode == 0) ? hmg_speed : aam_speed;
            Vec3 forward = {-sinf(cam_yaw) * cosf(cam_pitch), -sinf(cam_pitch),
                            cosf(cam_yaw) * cosf(cam_pitch)};

            projectiles[i].vel.x = player_vel.x + (forward.x * speed);
            projectiles[i].vel.y = player_vel.y + (forward.y * speed);
            projectiles[i].vel.z = player_vel.z + (forward.z * speed);

            projectiles[i].target_id =
                (weapon_mode == 1) ? locked_target_idx : -1;

            if (weapon_mode == 0)
              sfx_hmg = 255;

            break;
          }
        }
      }
    }

    // player flight controls
    Vec3 cam_forward = {-sinf(cam_yaw) * cosf(cam_pitch), -sinf(cam_pitch),
                        cosf(cam_yaw) * cosf(cam_pitch)};
    Vec3 cam_right = {cosf(cam_yaw), 0, sinf(cam_yaw)};

    if (keys[SDL_SCANCODE_W]) {
      player_vel.x += cam_forward.x * thrust;
      player_vel.y += cam_forward.y * thrust;
      player_vel.z += cam_forward.z * thrust;
    }
    if (keys[SDL_SCANCODE_S]) {
      player_vel.x -= cam_forward.x * thrust;
      player_vel.y -= cam_forward.y * thrust;
      player_vel.z -= cam_forward.z * thrust;
    }
    if (keys[SDL_SCANCODE_D]) {
      player_vel.x += cam_right.x * thrust;
      player_vel.y += cam_right.y * thrust;
      player_vel.z += cam_right.z * thrust;
    }
    if (keys[SDL_SCANCODE_A]) {
      player_vel.x -= cam_right.x * thrust;
      player_vel.y -= cam_right.y * thrust;
      player_vel.z -= cam_right.z * thrust;
    }

    player_vel.x *= 0.99f;
    player_vel.y *= 0.99f;
    player_vel.z *= 0.99f;
    player_pos.x += player_vel.x * dt;
    player_pos.y += player_vel.y * dt;
    player_pos.z += player_vel.z * dt;

    // targeting system
    if (aam_cooldown > 0.0f)
      aam_cooldown -= dt;
    if (weapon_mode == 1) {
      int best_target_this_frame = -1;
      float best_dot = -1.0f;
      int previous_lock = locked_target_idx;

      for (int t = 0; t < MAX_TARGETS; t++) {
        if (targets[t].active) {
          Vec3 dir_to_target = {targets[t].pos.x - player_pos.x,
                                targets[t].pos.y - player_pos.y,
                                targets[t].pos.z - player_pos.z};

          float dist = sqrtf(dir_to_target.x * dir_to_target.x +
                             dir_to_target.y * dir_to_target.y +
                             dir_to_target.z * dir_to_target.z);

          if (dist < MAX_LOCK_DIST) {
            Vec3 norm_dir = {dir_to_target.x / dist, dir_to_target.y / dist,
                             dir_to_target.z / dist};
            float d = dot_product(cam_forward, norm_dir);

            if (d > best_dot && d > 0.95f) {
              best_dot = d;
              best_target_this_frame = t;
            }
          }
        }
      }

      if (best_target_this_frame != -1) {
        if (best_target_this_frame == current_locking_target) {
          lock_timer += dt;

          audio_lock_ratio = lock_timer / TIME_TO_LOCK;
          audio_locked = 0;

          if (lock_timer >= TIME_TO_LOCK) {
            locked_target_idx = current_locking_target;
            audio_locked = 1;
            audio_lock_ratio = 1.0f;
          }
        } else {
          current_locking_target = best_target_this_frame;
          lock_timer = 0.0f;
          locked_target_idx = -1;
          audio_lock_ratio = 0.0f;
          audio_locked = 0;
        }
      } else {
        current_locking_target = -1;
        locked_target_idx = -1;
        lock_timer = 0.0f;
        audio_lock_ratio = 0.0f;
        audio_locked = 0;
      }

      if (previous_lock != -1 && locked_target_idx == -1) {
        lost_target_idx = previous_lock;
        lost_track_timer = 1.5f;
      }

    } else {
      current_locking_target = -1;
      locked_target_idx = -1;
      lock_timer = 0.0f;
      lost_track_timer = 0.0f;
      audio_lock_ratio = 0.0f;
      audio_locked = 0;
    }

    // physics
    for (int i = 0; i < MAX_TARGETS; i++) {
      if (targets[i].active) {
        targets[i].pos.x += targets[i].vel.x * dt;
        targets[i].pos.y += targets[i].vel.y * dt;
        targets[i].pos.z += targets[i].vel.z * dt;

        if (targets[i].pos.x > bound_size)
          targets[i].pos.x = -bound_size;
        if (targets[i].pos.x < -bound_size)
          targets[i].pos.x = bound_size;
        if (targets[i].pos.y > bound_size)
          targets[i].pos.y = -bound_size;
        if (targets[i].pos.y < -bound_size)
          targets[i].pos.y = bound_size;
        if (targets[i].pos.z > bound_size)
          targets[i].pos.z = -bound_size;
        if (targets[i].pos.z < -bound_size)
          targets[i].pos.z = bound_size;
      }
    }

    // projectile handling
    for (int p = 0; p < MAX_PROJ; p++) {
      if (projectiles[p].active) {

        if (projectiles[p].type == 1 && projectiles[p].target_id != -1) {
          Target *t = &targets[projectiles[p].target_id];

          if (t->active) {
            Vec3 to_t = {t->pos.x - projectiles[p].pos.x,
                         t->pos.y - projectiles[p].pos.y,
                         t->pos.z - projectiles[p].pos.z};
            float dist =
                sqrtf(to_t.x * to_t.x + to_t.y * to_t.y + to_t.z * to_t.z);
            Vec3 dir_to_target = normalize(to_t);
            Vec3 current_dir = normalize(projectiles[p].vel);

            float seeker_angle = dot_product(current_dir, dir_to_target);
            if (seeker_angle < 0.5f) {
              projectiles[p].target_id = -1;
            } else {
              float time_to_impact = dist / aam_speed;
              Vec3 predicted_pos = {t->pos.x + (t->vel.x * time_to_impact),
                                    t->pos.y + (t->vel.y * time_to_impact),
                                    t->pos.z + (t->vel.z * time_to_impact)};

              Vec3 lead_to_t = {predicted_pos.x - projectiles[p].pos.x,
                                predicted_pos.y - projectiles[p].pos.y,
                                predicted_pos.z - projectiles[p].pos.z};
              Vec3 desired_dir = normalize(lead_to_t);

              float turn_sharpness = 4.0f;
              Vec3 new_dir = {current_dir.x + (desired_dir.x - current_dir.x) *
                                                  turn_sharpness * dt,
                              current_dir.y + (desired_dir.y - current_dir.y) *
                                                  turn_sharpness * dt,
                              current_dir.z + (desired_dir.z - current_dir.z) *
                                                  turn_sharpness * dt};

              new_dir = normalize(new_dir);
              projectiles[p].vel.x = new_dir.x * aam_speed;
              projectiles[p].vel.y = new_dir.y * aam_speed;
              projectiles[p].vel.z = new_dir.z * aam_speed;
            }
          } else {
            projectiles[p].target_id = -1;
          }
        }

        projectiles[p].pos.x += projectiles[p].vel.x * dt;
        projectiles[p].pos.y += projectiles[p].vel.y * dt;
        projectiles[p].pos.z += projectiles[p].vel.z * dt;

        if (projectiles[p].type == 1) {
          projectiles[p].ttl -= dt;
          if (projectiles[p].ttl <= 0.0f) {
            projectiles[p].active = 0;
          }

          for (int pt = 0; pt < MAX_PARTICLES; pt++) {
            if (!particles[pt].active) {
              particles[pt].active = 1;
              particles[pt].pos = projectiles[p].pos;
              particles[pt].vel = (Vec3){0, 0, 0};
              particles[pt].life = 0.5f;
              break;
            }
          }
        }

        if (sqrtf(powf(projectiles[p].pos.x - player_pos.x, 2) +
                  powf(projectiles[p].pos.z - player_pos.z, 2)) > 500.0f) {
          projectiles[p].active = 0;
        }

        for (int t = 0; t < MAX_TARGETS; t++) {
          if (targets[t].active) {
            float dx = projectiles[p].pos.x - targets[t].pos.x;
            float dy = projectiles[p].pos.y - targets[t].pos.y;
            float dz = projectiles[p].pos.z - targets[t].pos.z;
            float tresh = projectiles[p].type == 1 ? 16.0f : 3.0f;
            if (sqrtf(dx * dx + dy * dy + dz * dz) < tresh) {
              targets[t].active = 0;
              projectiles[p].active = 0;

              sfx_exp = 255;

              int particles_to_spawn = 40;
              for (int pt = 0; pt < MAX_PARTICLES && particles_to_spawn > 0;
                   pt++) {
                if (!particles[pt].active) {
                  particles[pt].active = 1;
                  particles[pt].pos = targets[t].pos;
                  float rx = ((rand() % 100) / 50.0f) - 1.0f;
                  float ry = ((rand() % 100) / 50.0f) - 1.0f;
                  float rz = ((rand() % 100) / 50.0f) - 1.0f;
                  particles[pt].vel =
                      (Vec3){rx * 30.0f, ry * 30.0f, rz * 30.0f};
                  particles[pt].life = 0.5f + ((rand() % 100) / 200.0f);
                  particles_to_spawn--;
                }
              }

              if (current_locking_target == t) {
                current_locking_target = -1;
                audio_lock_ratio = 0.0f;
                audio_locked = 0;
              }
              if (locked_target_idx == t) {
                locked_target_idx = -1;
                audio_locked = 0;
              }
              if (lost_target_idx == t)
                lost_track_timer = 0.0f;

              targets[t].active = 1;
              targets[t].pos = (Vec3){player_pos.x + (rand() % 4000 - 2000),
                                      player_pos.y + (rand() % 4000 - 2000),
                                      player_pos.z + (rand() % 4000 - 2000)};
            }
          }
        }
      }
    }

    for (int pt = 0; pt < MAX_PARTICLES; pt++) {
      if (particles[pt].active) {
        particles[pt].pos.x += particles[pt].vel.x * dt;
        particles[pt].pos.y += particles[pt].vel.y * dt;
        particles[pt].pos.z += particles[pt].vel.z * dt;
        particles[pt].life -= dt;
        if (particles[pt].life <= 0.0f)
          particles[pt].active = 0;
      }
    }

// rendering
#if OLED == 1
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
#else
    SDL_SetRenderDrawColor(renderer, 15, 18, 28, 255);
#endif
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255);
    for (int i = 0; i < NUM_STARS; i++) {
      float dx = stars[i].x - player_pos.x;
      float dy = stars[i].y - player_pos.y;
      float dz = stars[i].z - player_pos.z;

      if (dx > 2000.0f)
        stars[i].x -= 4000.0f;
      if (dx < -2000.0f)
        stars[i].x += 4000.0f;
      if (dy > 2000.0f)
        stars[i].y -= 4000.0f;
      if (dy < -2000.0f)
        stars[i].y += 4000.0f;
      if (dz > 2000.0f)
        stars[i].z -= 4000.0f;
      if (dz < -2000.0f)
        stars[i].z += 4000.0f;

      Vec3 cam_star = apply_camera(stars[i], player_pos, cam_yaw, cam_pitch);
      if (cam_star.z > 0.1f) {
        Vec2 s = screen(project_persp(cam_star));
        SDL_RenderDrawPoint(renderer, (int)s.x, (int)s.y);
      }
    }

    SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255);
    draw_planet(0,player_pos,cam_yaw,cam_pitch);
    SDL_SetRenderDrawColor(renderer, 150, 80, 50, 255);
    draw_planet(1,player_pos,cam_yaw,cam_pitch);;

    SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
    for (int t = 0; t < MAX_TARGETS; t++) {
      if (!targets[t].active)
        continue;
      float target_yaw = atan2f(-targets[t].vel.x, targets[t].vel.z);
      for (int f_idx = 0; f_idx < NUM_FACES; ++f_idx) {
        for (int i = 0; i < spaceship_fs[f_idx].count; ++i) {
          int v1 = spaceship_fs[f_idx].v[i];
          int v2 = spaceship_fs[f_idx].v[(i + 1) % spaceship_fs[f_idx].count];
          Vec3 r_p1 = rotate_xz(spaceship_vs[v1], target_yaw);
          Vec3 r_p2 = rotate_xz(spaceship_vs[v2], target_yaw);
          Vec3 w_p1 = {r_p1.x + targets[t].pos.x, r_p1.y + targets[t].pos.y,
                       r_p1.z + targets[t].pos.z};
          Vec3 w_p2 = {r_p2.x + targets[t].pos.x, r_p2.y + targets[t].pos.y,
                       r_p2.z + targets[t].pos.z};
          draw_3d_line(w_p1, w_p2, player_pos, cam_yaw, cam_pitch);
        }
      }
    }

    for (int p = 0; p < MAX_PROJ; p++) {
      if (projectiles[p].active) {
        if (projectiles[p].type == 0)
          SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
        else
          SDL_SetRenderDrawColor(renderer, 255, 100, 255, 255);

        Vec3 p_start = projectiles[p].pos;
        Vec3 p_end = {p_start.x - (projectiles[p].vel.x * 0.05f),
                      p_start.y - (projectiles[p].vel.y * 0.05f),
                      p_start.z - (projectiles[p].vel.z * 0.05f)};
        draw_3d_line(p_start, p_end, player_pos, cam_yaw, cam_pitch);
      }
    }

    SDL_SetRenderDrawColor(renderer, 255, 150, 50, 255);
    for (int pt = 0; pt < MAX_PARTICLES; pt++) {
      if (particles[pt].active) {
        Vec3 p_start = particles[pt].pos;
        Vec3 p_end = {p_start.x - (particles[pt].vel.x * 0.02f),
                      p_start.y - (particles[pt].vel.y * 0.02f),
                      p_start.z - (particles[pt].vel.z * 0.02f)};
        if (particles[pt].vel.x == 0) {
          p_end.x += 0.5f;
        }
        draw_3d_line(p_start, p_end, player_pos, cam_yaw, cam_pitch);
      }
    }

    // hud
    if (current_locking_target != -1) {
      Target t = targets[current_locking_target];
      Vec3 cam_aim = apply_camera(t.pos, player_pos, cam_yaw, cam_pitch);
      if (cam_aim.z > 0.1f) {
        Vec2 s = screen(project_persp(cam_aim));

        if (locked_target_idx != -1) {
          SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
          SDL_RenderDrawLine(renderer, s.x - 15, s.y - 15, s.x + 15, s.y - 15);
          SDL_RenderDrawLine(renderer, s.x + 15, s.y - 15, s.x + 15, s.y + 15);
          SDL_RenderDrawLine(renderer, s.x + 15, s.y + 15, s.x - 15, s.y + 15);
          SDL_RenderDrawLine(renderer, s.x - 15, s.y + 15, s.x - 15, s.y - 15);
          draw_vector_string(renderer, "LOCKED", s.x - 20, s.y + 25, 1.5f);
        } else {
          SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
          float box_size = 20.0f + ((TIME_TO_LOCK - lock_timer) * 20.0f);

          SDL_RenderDrawLine(renderer, s.x - box_size, s.y - box_size, s.x - 10,
                             s.y - box_size);
          SDL_RenderDrawLine(renderer, s.x - box_size, s.y - box_size,
                             s.x - box_size, s.y - 10);
          SDL_RenderDrawLine(renderer, s.x + box_size, s.y - box_size, s.x + 10,
                             s.y - box_size);
          SDL_RenderDrawLine(renderer, s.x + box_size, s.y - box_size,
                             s.x + box_size, s.y - 10);
          SDL_RenderDrawLine(renderer, s.x - box_size, s.y + box_size, s.x - 10,
                             s.y + box_size);
          SDL_RenderDrawLine(renderer, s.x - box_size, s.y + box_size,
                             s.x - box_size, s.y + 10);
          SDL_RenderDrawLine(renderer, s.x + box_size, s.y + box_size, s.x + 10,
                             s.y + box_size);
          SDL_RenderDrawLine(renderer, s.x + box_size, s.y + box_size,
                             s.x + box_size, s.y + 10);
          draw_vector_string(renderer, "LOCKING", s.x - 25, s.y + box_size + 10,
                             1.5f);
        }
      }
    }

    if (lost_track_timer > 0.0f) {
      lost_track_timer -= dt;
      Target t = targets[lost_target_idx];
      Vec3 cam_aim = apply_camera(t.pos, player_pos, cam_yaw, cam_pitch);
      if (cam_aim.z > 0.1f) {
        Vec2 s = screen(project_persp(cam_aim));
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        float b = 25.0f;
        SDL_RenderDrawLine(renderer, s.x - b, s.y - b, s.x - b / 2, s.y - b);
        SDL_RenderDrawLine(renderer, s.x - b, s.y - b, s.x - b, s.y - b / 2);
        SDL_RenderDrawLine(renderer, s.x + b, s.y - b, s.x + b / 2, s.y - b);
        SDL_RenderDrawLine(renderer, s.x + b, s.y - b, s.x + b, s.y - b / 2);
        SDL_RenderDrawLine(renderer, s.x - b, s.y + b, s.x - b / 2, s.y + b);
        SDL_RenderDrawLine(renderer, s.x - b, s.y + b, s.x - b, s.y + b / 2);
        SDL_RenderDrawLine(renderer, s.x + b, s.y + b, s.x + b / 2, s.y + b);
        SDL_RenderDrawLine(renderer, s.x + b, s.y + b, s.x + b, s.y + b / 2);

        draw_vector_string(renderer, "LOST TRACK", s.x - 30, s.y + b + 10,
                           1.5f);
      }
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawLine(renderer, width / 2 - 15, height / 2, width / 2 - 5,
                       height / 2);
    SDL_RenderDrawLine(renderer, width / 2 + 5, height / 2, width / 2 + 15,
                       height / 2);
    SDL_RenderDrawLine(renderer, width / 2, height / 2 - 15, width / 2,
                       height / 2 - 5);
    SDL_RenderDrawLine(renderer, width / 2, height / 2 + 5, width / 2,
                       height / 2 + 15);

    char hud_data[64];
    float speed =
        sqrtf(player_vel.x * player_vel.x + player_vel.y * player_vel.y +
              player_vel.z * player_vel.z);
    sprintf(hud_data, "SPD: %.0f - P: %f | Y: %f", speed, cam_pitch, cam_yaw);

    draw_vector_string(renderer, hud_data, 30, height - 50, 3.0f);

    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    draw_vector_string(renderer, "HMG   OK", width - 180, 40, 3.0f);
    const char *aam_ok = "AAM   OK";
    const char *aam_reload = "AAM WAIT";
    draw_vector_string(renderer, aam_cooldown <= 0.0f ? aam_ok : aam_reload,
                       width - 180, 80, 3.0f);

    SDL_Rect wpn_box;
    wpn_box.x = width - 190;
    wpn_box.y = (weapon_mode == 0) ? 30 : 70;
    wpn_box.w = 160;
    wpn_box.h = 35;
    SDL_RenderDrawRect(renderer, &wpn_box);

    SDL_RenderPresent(renderer);
    SDL_Delay(1000 / FPS_TARGET);
  }

  SDL_CloseAudioDevice(audio_dev);

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}