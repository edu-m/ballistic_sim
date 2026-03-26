#ifndef STRUCTS_H
#define STRUCTS_H

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
  int type; // 0 = HMG, 1 = AAM
  float ttl;
  int target_id;
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

#endif