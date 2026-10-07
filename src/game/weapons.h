#ifndef VX_WEAPONS_H
#define VX_WEAPONS_H

#include "game/flight.h"
#include <stdbool.h>

#define VX_MAX_PROJECTILES 32
#define VX_FIRST_REPEAT_DELAY (3.0f / 30.0f)
#define VX_FIRE_INTERVAL (4.0f / 30.0f)
#define VX_BURST_SHOTS 3
#define VX_CHARGE_TIME (20.0f / 30.0f)
#define VX_PROJECTILE_SPEED 45.0f
#define VX_CHARGED_PROJECTILE_SPEED 35.0f
#define VX_PROJECTILE_LIMIT 100.0f

typedef enum { VX_PROJECTILE_LASER, VX_PROJECTILE_CHARGED } VxProjectileKind;
typedef enum { VX_WEAPON_IDLE, VX_WEAPON_BURST, VX_WEAPON_CHARGING, VX_WEAPON_READY } VxWeaponState;
typedef struct { VxVec3 position; bool active; VxProjectileKind kind; } VxProjectile;
typedef struct {
    VxProjectile projectiles[VX_MAX_PROJECTILES];
    float cooldown, charge_time;
    int burst_shots;
    bool was_firing;
    VxWeaponState state;
} VxWeapons;

void vx_weapons_reset(VxWeapons *weapons);
void vx_weapons_cancel_trigger(VxWeapons *weapons);
void vx_weapons_update(VxWeapons *weapons, const VxFlight *flight, bool firing, float dt);
float vx_weapons_charge_fraction(const VxWeapons *weapons);
VxVec3 vx_weapons_charge_origin(const VxFlight *flight);

#endif
