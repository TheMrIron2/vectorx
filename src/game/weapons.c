#include "game/weapons.h"

void vx_weapons_reset(VxWeapons *weapons) { *weapons = (VxWeapons){0}; }

static void fire(VxWeapons *weapons, const VxFlight *flight) {
    int slots[2], count = 0;
    for (int i = 0; i < VX_MAX_PROJECTILES && count < 2; ++i)
        if (!weapons->projectiles[i].active) slots[count++] = i;
    if (count != 2) return; /* A full pool cannot spawn a partial pair. */
    for (int i = 0; i < 2; ++i) {
        const float side = i == 0 ? -1.0f : 1.0f;
        VxVec3 muzzle = vx_rotate((VxVec3){side * 0.65f, 0.13f, -0.18f},
                                 flight->pitch, flight->yaw, flight->roll);
        muzzle = vx_v3_add(muzzle, (VxVec3){flight->position.x, flight->position.y, 8});
        weapons->projectiles[slots[i]] = (VxProjectile){muzzle, true};
    }
}

void vx_weapons_update(VxWeapons *weapons, const VxFlight *flight, bool firing, float dt) {
    if (!isfinite(dt) || dt <= 0) return;
    for (int i = 0; i < VX_MAX_PROJECTILES; ++i) {
        VxProjectile *projectile = &weapons->projectiles[i];
        if (!projectile->active) continue;
        /* Travel along the rail; movement/banking affects the firing origin. */
        projectile->position.z += VX_PROJECTILE_SPEED * dt;
        if (projectile->position.z > VX_PROJECTILE_LIMIT) projectile->active = false;
    }
    weapons->cooldown = fmaxf(0, weapons->cooldown - dt);
    if (firing && weapons->cooldown <= 0.000001f) {
        fire(weapons, flight);
        weapons->cooldown = VX_FIRE_INTERVAL;
    }
}
