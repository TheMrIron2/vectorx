#include "game/weapons.h"

void vx_weapons_reset(VxWeapons *weapons) { *weapons = (VxWeapons){0}; }

void vx_weapons_cancel_trigger(VxWeapons *weapons) {
    weapons->cooldown = weapons->charge_time = 0;
    weapons->burst_shots = 0;
    weapons->was_firing = false;
    weapons->state = VX_WEAPON_IDLE;
}

static bool fire_lasers(VxWeapons *weapons, const VxFlight *flight) {
    int slots[2], count = 0;
    for (int i = 0; i < VX_MAX_PROJECTILES && count < 2; ++i)
        if (!weapons->projectiles[i].active) slots[count++] = i;
    if (count != 2) return false; /* A full pool cannot spawn a partial pair. */
    for (int i = 0; i < 2; ++i) {
        const float side = i == 0 ? -1.0f : 1.0f;
        VxVec3 muzzle = vx_rotate((VxVec3){side * 0.65f, 0.13f, -0.18f},
                                 flight->pitch, flight->yaw, flight->roll);
        muzzle = vx_v3_add(muzzle, (VxVec3){flight->position.x, flight->position.y, 8});
        weapons->projectiles[slots[i]] = (VxProjectile){muzzle, true, VX_PROJECTILE_LASER};
    }
    return true;
}

VxVec3 vx_weapons_charge_origin(const VxFlight *flight) {
    return vx_v3_add(vx_rotate((VxVec3){0, 0.12f, 1.9f}, flight->pitch, flight->yaw, flight->roll),
        (VxVec3){flight->position.x, flight->position.y, 8});
}

static void fire_charged(VxWeapons *weapons, const VxFlight *flight) {
    for (int i = 0; i < VX_MAX_PROJECTILES; ++i) {
        if (weapons->projectiles[i].active) continue;
        weapons->projectiles[i] = (VxProjectile){vx_weapons_charge_origin(flight), true, VX_PROJECTILE_CHARGED};
        return;
    }
}

float vx_weapons_charge_fraction(const VxWeapons *weapons) {
    if (weapons->state == VX_WEAPON_READY) return 1;
    if (weapons->state != VX_WEAPON_CHARGING) return 0;
    const float burst_duration = VX_FIRST_REPEAT_DELAY + VX_FIRE_INTERVAL;
    return vx_clamp((weapons->charge_time - burst_duration) / (VX_CHARGE_TIME - burst_duration), 0, 1);
}

void vx_weapons_update(VxWeapons *weapons, const VxFlight *flight, bool firing, float dt) {
    if (!isfinite(dt) || dt <= 0) return;
    for (int i = 0; i < VX_MAX_PROJECTILES; ++i) {
        VxProjectile *projectile = &weapons->projectiles[i];
        if (!projectile->active) continue;
        /* Travel along the rail; movement/banking affects the firing origin. */
        const float speed = projectile->kind == VX_PROJECTILE_CHARGED
            ? VX_CHARGED_PROJECTILE_SPEED : VX_PROJECTILE_SPEED;
        projectile->position.z += speed * dt;
        if (projectile->position.z > VX_PROJECTILE_LIMIT) projectile->active = false;
    }
    if (!firing) {
        if (weapons->was_firing && weapons->state == VX_WEAPON_READY) fire_charged(weapons, flight);
        vx_weapons_cancel_trigger(weapons);
        return;
    }
    if (!weapons->was_firing) {
        weapons->state = VX_WEAPON_BURST;
        weapons->burst_shots = 0;
        weapons->charge_time = weapons->cooldown = 0;
    }
    weapons->was_firing = true;
    weapons->charge_time = fminf(VX_CHARGE_TIME, weapons->charge_time + dt);
    weapons->cooldown = fmaxf(0, weapons->cooldown - dt);
    if (weapons->state == VX_WEAPON_BURST && weapons->cooldown <= 0.000001f) {
        if (fire_lasers(weapons, flight)) {
            ++weapons->burst_shots;
            if (weapons->burst_shots == VX_BURST_SHOTS) weapons->state = VX_WEAPON_CHARGING;
        }
        weapons->cooldown = weapons->burst_shots == 1 ? VX_FIRST_REPEAT_DELAY : VX_FIRE_INTERVAL;
    }
    if (weapons->state == VX_WEAPON_CHARGING && weapons->charge_time >= VX_CHARGE_TIME - 0.000001f)
        weapons->state = VX_WEAPON_READY;
}
