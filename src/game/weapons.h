#ifndef VX_WEAPONS_H
#define VX_WEAPONS_H

#include "game/flight.h"
#include <stdbool.h>

#define VX_MAX_PROJECTILES 32
#define VX_FIRE_INTERVAL 0.15f
#define VX_PROJECTILE_SPEED 45.0f
#define VX_PROJECTILE_LIMIT 100.0f

typedef struct { VxVec3 position; bool active; } VxProjectile;
typedef struct {
    VxProjectile projectiles[VX_MAX_PROJECTILES];
    float cooldown;
} VxWeapons;

void vx_weapons_reset(VxWeapons *weapons);
void vx_weapons_update(VxWeapons *weapons, const VxFlight *flight, bool firing, float dt);

#endif
