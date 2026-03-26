#include "font.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define GRAV_CONST 9.8182f
#define FPS_TARGET 180
int width = 1200;
int height = 1200;
SDL_Renderer *renderer;

typedef struct {
  float x, y, z;
} Vec3;
typedef struct {
  float x, y;
} Vec2;
typedef struct {
  int count;
  int v[4];
} Face;

typedef struct {
  int active;
  Vec3 pos;
  Vec3 vel;
} Projectile;
typedef struct {
  int active;
  Vec3 pos;
  Vec3 vel;
} Target;
typedef struct {
  int active;
  Vec3 pos;
  Vec3 vel;
  float life;
} Particle;

const int NUM_VERTICES = 14;
Vec3 spaceship_vs[14] = {
    {0.0f, 0.0f, 1.5f},   {-0.5f, 0.2f, 0.5f},   {0.5f, 0.2f, 0.5f},
    {-0.5f, -0.2f, 0.5f}, {0.5f, -0.2f, 0.5f},   {-1.0f, 0.5f, -1.0f},
    {1.0f, 0.5f, -1.0f},  {-1.0f, -0.5f, -1.0f}, {1.0f, -0.5f, -1.0f},
    {-2.5f, 1.0f, 0.0f},  {2.5f, 1.0f, 0.0f},    {-2.5f, -1.0f, 0.0f},
    {2.5f, -1.0f, 0.0f},  {0.0f, 0.0f, -1.2f}};

const int NUM_FACES = 28;
Face spaceship_fs[28] = {{2, {0, 1}},  {2, {0, 2}},  {2, {0, 3}},  {2, {0, 4}},
               {2, {1, 2}},  {2, {2, 4}},  {2, {4, 3}},  {2, {3, 1}},
               {2, {1, 5}},  {2, {2, 6}},  {2, {3, 7}},  {2, {4, 8}},
               {2, {5, 6}},  {2, {6, 8}},  {2, {8, 7}},  {2, {7, 5}},
               {2, {5, 9}},  {2, {9, 1}},  {2, {6, 10}}, {2, {10, 2}},
               {2, {7, 11}}, {2, {11, 3}}, {2, {8, 12}}, {2, {12, 4}},
               {2, {5, 13}}, {2, {6, 13}}, {2, {7, 13}}, {2, {8, 13}}};

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

Vec3 apply_camera(Vec3 v, float cam_yaw, float cam_pitch) {
  float cy = cosf(-cam_yaw), sy = sinf(-cam_yaw);
  float x1 = v.x * cy - v.z * sy;
  float z1 = v.x * sy + v.z * cy;

  float cp = cosf(-cam_pitch), sp = sinf(-cam_pitch);
  float y2 = v.y * cp - z1 * sp;
  float z2 = v.y * sp + z1 * cp;

  return (Vec3){x1, y2, z2};
}

Vec3 rotate_xz(Vec3 v, float angle) {
  float c = cosf(angle);
  float s = sinf(angle);
  return (Vec3){v.x * c - v.z * s, v.y, v.x * s + v.z * c};
}

Vec3 rotate_yz(Vec3 v, float angle) {
  float c = cosf(angle);
  float s = sinf(angle);
  return (Vec3){v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}
Vec3 rotate_xy(Vec3 v, float angle) {
  float c = cosf(angle);
  float s = sinf(angle);
  return (Vec3){v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

float dot_product(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 normalize(Vec3 v) {
  float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len == 0.0f)
    return (Vec3){0, 0, 1};
  return (Vec3){v.x / len, v.y / len, v.z / len};
}

void draw_3d_line(Vec3 w_a, Vec3 w_b, float cam_yaw, float cam_pitch) {
  Vec3 c_a = apply_camera(w_a, cam_yaw, cam_pitch);
  Vec3 c_b = apply_camera(w_b, cam_yaw, cam_pitch);

  float near_z = 0.1f;

  if (c_a.z < near_z && c_b.z < near_z)
    return;

  if (c_a.z < near_z) {
    float t = (near_z - c_a.z) / (c_b.z - c_a.z);
    c_a.x = c_a.x + t * (c_b.x - c_a.x);
    c_a.y = c_a.y + t * (c_b.y - c_a.y);
    c_a.z = near_z;
  } else if (c_b.z < near_z) {
    float t = (near_z - c_b.z) / (c_a.z - c_b.z);
    c_b.x = c_b.x + t * (c_a.x - c_b.x);
    c_b.y = c_b.y + t * (c_a.y - c_b.y);
    c_b.z = near_z;
  }

  Vec2 p_a = screen(project_persp(c_a));
  Vec2 p_b = screen(project_persp(c_b));
  SDL_RenderDrawLine(renderer, (int)p_a.x, (int)p_a.y, (int)p_b.x, (int)p_b.y);
}

void clamp_wraparound(Vec3 *v, float val) {
    v->x = (v->x > val) ? -val : (v->x < -val) ? val : v->x;
    v->y = (v->y > val) ? -val : (v->y < -val) ? val : v->y;
    v->z = (v->z > val) ? -val : (v->z < -val) ? val : v->z;
}

int main() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    return 1;

  SDL_Window *window =
      SDL_CreateWindow("Ballistic Simulator", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_SetRelativeMouseMode(SDL_TRUE);
  SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
  int running = 1;
  SDL_Event event;
  SDL_GetWindowSize(window, &width, &height);
  float cam_yaw = 0.0f;
  float cam_pitch = 0.2f;
  float mouse_sensitivity = 0.003f;

  float weapon_speed = 50.0f;
  int show_indicator = 0;

#define MAX_PROJ 30
#define MAX_TARGETS 5
#define MAX_PARTICLES 200

  Projectile projectiles[MAX_PROJ] = {0};
  Target targets[MAX_TARGETS] = {0};
  Particle particles[MAX_PARTICLES] = {0};

  const float max_dist = 200.0f;
  for (int i = 0; i < MAX_TARGETS; i++) {
    targets[i].active = 1;
    targets[i].pos = (Vec3){(rand() % 40 - 20), 15.0f + (rand() % 10),
                            20.0f + (rand() % 30)};
    targets[i].vel =
        (Vec3){((rand() % 15)) - 1.0f, 0.0f, ((rand() % 15)) - 1.0f};
  }

  float dt = 1.0f / 60;

  while (running) {

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;

      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE)
          running = 0;
        if (event.key.keysym.sym == SDLK_i)
          show_indicator = !show_indicator;
      }

      if (event.type == SDL_MOUSEMOTION) {
        cam_yaw -= event.motion.xrel * mouse_sensitivity;
        cam_pitch += event.motion.yrel * mouse_sensitivity;
        if (cam_pitch > M_PI / 2.01f)
          cam_pitch = M_PI / 2.01f;
        if (cam_pitch < -M_PI / 2.01f)
          cam_pitch = -M_PI / 2.01f;
        if (cam_yaw > 2 * M_PI)
          cam_yaw -= 2 * M_PI;
        if (cam_yaw < 0)
          cam_yaw += 2 * M_PI;
      }

      if (event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_LEFT) {
        for (int i = 0; i < MAX_PROJ; i++) {
          if (!projectiles[i].active) {
            projectiles[i].active = 1;
            projectiles[i].pos = (Vec3){0, 0, 0};

            projectiles[i].vel.x =
                -weapon_speed * sinf(cam_yaw) * cosf(cam_pitch);
            projectiles[i].vel.y = -weapon_speed * sinf(cam_pitch);
            projectiles[i].vel.z =
                weapon_speed * cosf(cam_yaw) * cosf(cam_pitch);
            break;
          }
        }
      }
    }

    for (int i = 0; i < MAX_TARGETS; i++) {
      if (targets[i].active) {
        targets[i].pos.x += targets[i].vel.x * dt;
        targets[i].pos.z += targets[i].vel.z * dt;
        clamp_wraparound(&targets[i].pos, max_dist);
      }
    }

    for (int pt = 0; pt < MAX_PARTICLES; pt++) {
      if (particles[pt].active) {
        particles[pt].pos.x += particles[pt].vel.x * dt;
        particles[pt].pos.y += particles[pt].vel.y * dt;
        particles[pt].pos.z += particles[pt].vel.z * dt;

        particles[pt].vel.y -= GRAV_CONST * dt;
        particles[pt].life -= dt;

        if (particles[pt].life <= 0.0f || particles[pt].pos.y < -5.0f) {
          particles[pt].active = 0;
        }
      }
    }

    for (int p = 0; p < MAX_PROJ; p++) {
      if (projectiles[p].active) {
        projectiles[p].pos.x += projectiles[p].vel.x * dt;
        projectiles[p].pos.y += projectiles[p].vel.y * dt;
        projectiles[p].pos.z += projectiles[p].vel.z * dt;
        projectiles[p].vel.y -= GRAV_CONST * dt;

        if (projectiles[p].pos.y < -5.0f)
          projectiles[p].active = 0;

        for (int t = 0; t < MAX_TARGETS; t++) {
          if (targets[t].active) {
            float dx = projectiles[p].pos.x - targets[t].pos.x;
            float dy = projectiles[p].pos.y - targets[t].pos.y;
            float dz = projectiles[p].pos.z - targets[t].pos.z;
            if (sqrtf(dx * dx + dy * dy + dz * dz) < 2.0f) {
              targets[t].active = 0;
              projectiles[p].active = 0;

              int particles_to_spawn = 20;
              for (int pt = 0; pt < MAX_PARTICLES && particles_to_spawn > 0;
                   pt++) {
                if (!particles[pt].active) {
                  particles[pt].active = 1;
                  particles[pt].pos = targets[t].pos;

                  float rx = ((rand() % 100) / 50.0f) - 1.0f;
                  float ry = ((rand() % 100) / 50.0f) - 1.0f;
                  float rz = ((rand() % 100) / 50.0f) - 1.0f;

                  particles[pt].vel =
                      (Vec3){rx * 15.0f, ry * 15.0f, rz * 15.0f};

                  particles[pt].life = 0.5f + ((rand() % 100) / 200.0f);
                  particles_to_spawn--;
                }
              }

              targets[t].active = 1;
              targets[t].pos = (Vec3){(rand() % 40 - 20), 15.0f + (rand() % 10),
                                      20.0f + (rand() % 30)};
            }
          }
        }
      }
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 40, 80, 40, 255);
    float floor_y = -5.0f;
    for (int i = -100; i <= 100; i += 5) {
      draw_3d_line((Vec3){-100, floor_y, i}, (Vec3){100, floor_y, i}, cam_yaw,
                   cam_pitch);
      draw_3d_line((Vec3){i, floor_y, -100}, (Vec3){i, floor_y, 100}, cam_yaw,
                   cam_pitch);
    }

    SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
    for (int t = 0; t < MAX_TARGETS; t++) {
      if (!targets[t].active)
        continue;
      // vector angle to polar coordinates
      float target_dir = atan2f(-targets[t].vel.x, targets[t].vel.z);

      for (int f_idx = 0; f_idx < NUM_FACES; ++f_idx) {
        for (int i = 0; i < spaceship_fs[f_idx].count; ++i) {
          int v1 = spaceship_fs[f_idx].v[i];
          int v2 = spaceship_fs[f_idx].v[(i + 1) % spaceship_fs[f_idx].count];

          Vec3 r_p1 = rotate_xz(spaceship_vs[v1], target_dir);
          Vec3 r_p2 = rotate_xz(spaceship_vs[v2], target_dir);

          Vec3 w_p1 = {r_p1.x + targets[t].pos.x, r_p1.y + targets[t].pos.y,
                       r_p1.z + targets[t].pos.z};
          Vec3 w_p2 = {r_p2.x + targets[t].pos.x, r_p2.y + targets[t].pos.y,
                       r_p2.z + targets[t].pos.z};

          draw_3d_line(w_p1, w_p2, cam_yaw, cam_pitch);
        }
      }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
    for (int p = 0; p < MAX_PROJ; p++) {
      if (projectiles[p].active) {
        Vec3 p_start = projectiles[p].pos;
        Vec3 p_end = {p_start.x - (projectiles[p].vel.x * 0.05f),
                      p_start.y - (projectiles[p].vel.y * 0.05f),
                      p_start.z - (projectiles[p].vel.z * 0.05f)};
        draw_3d_line(p_start, p_end, cam_yaw, cam_pitch);
      }
    }

    SDL_SetRenderDrawColor(renderer, 255, 150, 50, 255);
    for (int pt = 0; pt < MAX_PARTICLES; pt++) {
      if (particles[pt].active) {
        Vec3 p_start = particles[pt].pos;
        Vec3 p_end = {p_start.x - (particles[pt].vel.x * 0.02f),
                      p_start.y - (particles[pt].vel.y * 0.02f),
                      p_start.z - (particles[pt].vel.z * 0.02f)};
        draw_3d_line(p_start, p_end, cam_yaw, cam_pitch);
      }
    }

    if (show_indicator) {
      int locked_target_idx = -1;
      float best_dot = -1.0f;

      Vec3 forward = {-sinf(cam_yaw) * cosf(cam_pitch), -sinf(cam_pitch),
                      cosf(cam_yaw) * cosf(cam_pitch)};

      for (int t = 0; t < MAX_TARGETS; t++) {
        if (targets[t].active) {
          Vec3 dir_to_target = {targets[t].pos.x, targets[t].pos.y,
                                targets[t].pos.z};
          Vec3 norm_dir = normalize(dir_to_target);
          float d = dot_product(forward, norm_dir);

          if (d > best_dot && d > 0.5f) {
            best_dot = d;
            locked_target_idx = t;
          }
        }
      }

      if (locked_target_idx != -1) {
        Target locked_tgt = targets[locked_target_idx];

        float t_flight = sqrtf(locked_tgt.pos.x * locked_tgt.pos.x +
                               locked_tgt.pos.y * locked_tgt.pos.y +
                               locked_tgt.pos.z * locked_tgt.pos.z) /
                         weapon_speed;
        Vec3 aim_point;

        for (int iter = 0; iter < 10; iter++) {
          aim_point.x = locked_tgt.pos.x + locked_tgt.vel.x * t_flight;
          aim_point.y = locked_tgt.pos.y + locked_tgt.vel.y * t_flight +
                        0.5f * GRAV_CONST * t_flight * t_flight;
          aim_point.z = locked_tgt.pos.z + locked_tgt.vel.z * t_flight;

          float distance_to_aim =
              sqrtf(aim_point.x * aim_point.x + aim_point.y * aim_point.y +
                    aim_point.z * aim_point.z);
          t_flight = distance_to_aim / weapon_speed;
        }

        Vec3 cam_aim = apply_camera(aim_point, cam_yaw, cam_pitch);
        if (cam_aim.z > 0.1f) {
          Vec2 s = screen(project_persp(cam_aim));

          SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
          SDL_RenderDrawLine(renderer, s.x - 5, s.y - 5, s.x + 5, s.y + 5);
          SDL_RenderDrawLine(renderer, s.x - 5, s.y + 5, s.x + 5, s.y - 5);

          SDL_RenderDrawLine(renderer, s.x - 10, s.y - 10, s.x + 10, s.y - 10);
          SDL_RenderDrawLine(renderer, s.x + 10, s.y - 10, s.x + 10, s.y + 10);
          SDL_RenderDrawLine(renderer, s.x + 10, s.y + 10, s.x - 10, s.y + 10);
          SDL_RenderDrawLine(renderer, s.x - 10, s.y + 10, s.x - 10, s.y - 10);
        }
      }
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawLine(renderer, width / 2 - 10, height / 2, width / 2 + 10,
                       height / 2);
    SDL_RenderDrawLine(renderer, width / 2, height / 2 - 10, width / 2,
                       height / 2 + 10);

    char hud_data[64];
    sprintf(hud_data, "P: %.2f Y: %.2f", cam_pitch * 180 / M_PI,
            cam_yaw * 180 / M_PI);
    draw_vector_string(renderer, hud_data, 20, 50, 5.0f);

    SDL_RenderPresent(renderer);
    SDL_Delay(1000 / FPS_TARGET);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
