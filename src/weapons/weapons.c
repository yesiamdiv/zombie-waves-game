#include "weapons/weapons.h"
#include "events/event_bus.h"

void weapons_inventory_init(PlayerInventory *inv) {
    if (!inv) return;
    inv->current = WEAPON_PISTOL;
    for (int i = 0; i < WEAPON_COUNT; i++) inv->unlocked[i] = false;
    inv->unlocked[WEAPON_PISTOL] = true;
    inv->grenades = 0;
    inv->launcher_ammo = 0;
    inv->points = 0;
}

bool weapons_select(PlayerInventory *inv, WeaponType w) {
    if (!inv || w < 0 || w >= WEAPON_COUNT) return false;
    if (!inv->unlocked[w]) return false;
    inv->current = w;
    return true;
}

void weapons_award_kill(PlayerInventory *inv, int kills) {
    if (!inv || kills <= 0) return;
    int awarded = POINTS_PER_KILL * kills;
    inv->points += awarded;
    if (g_events) {
        event_emit(g_events, GE_POINTS, ECS_NULL_ENTITY, GEK_PLAYER,
                   0, 0, (float)inv->points, (float)awarded, kills, 0);
    }
}

bool weapons_buy_sword(PlayerInventory *inv) {
    if (!inv || inv->unlocked[WEAPON_SWORD]) return false;
    if (inv->points < SWORD_COST) return false;
    inv->points -= SWORD_COST;
    inv->unlocked[WEAPON_SWORD] = true;
    if (g_events) {
        event_emit(g_events, GE_SHOP_PURCHASE, ECS_NULL_ENTITY, GEK_PLAYER,
                   0, 0, (float)WEAPON_SWORD, (float)SWORD_COST, 1, 0);
    }
    return true;
}

bool weapons_buy_grenade_pack(PlayerInventory *inv) {
    if (!inv || inv->points < GRENADE_PACK_COST) return false;
    inv->points -= GRENADE_PACK_COST;
    inv->grenades += GRENADES_PER_PACK;
    inv->unlocked[WEAPON_GRENADE] = true;
    if (g_events) {
        event_emit(g_events, GE_SHOP_PURCHASE, ECS_NULL_ENTITY, GEK_PLAYER,
                   0, 0, (float)WEAPON_GRENADE, (float)GRENADE_PACK_COST,
                   GRENADES_PER_PACK, 0);
    }
    return true;
}

bool weapons_buy_launcher(PlayerInventory *inv) {
    if (!inv || inv->unlocked[WEAPON_LAUNCHER]) return false;
    if (inv->points < LAUNCHER_COST) return false;
    inv->points -= LAUNCHER_COST;
    inv->unlocked[WEAPON_LAUNCHER] = true;
    inv->launcher_ammo += LAUNCHER_STARTER_ROCKETS;
    if (g_events) {
        event_emit(g_events, GE_SHOP_PURCHASE, ECS_NULL_ENTITY, GEK_PLAYER,
                   0, 0, (float)WEAPON_LAUNCHER, (float)LAUNCHER_COST,
                   LAUNCHER_STARTER_ROCKETS, 0);
    }
    return true;
}

bool weapons_buy_launcher_ammo(PlayerInventory *inv) {
    if (!inv || inv->points < LAUNCHER_AMMO_COST) return false;
    if (!inv->unlocked[WEAPON_LAUNCHER]) return false;
    inv->points -= LAUNCHER_AMMO_COST;
    inv->launcher_ammo += ROCKETS_PER_PACK;
    if (g_events) {
        event_emit(g_events, GE_SHOP_PURCHASE, ECS_NULL_ENTITY, GEK_PLAYER,
                   0, 0, (float)WEAPON_LAUNCHER, (float)LAUNCHER_AMMO_COST,
                   ROCKETS_PER_PACK, 0);
    }
    return true;
}

const char *weapons_name(WeaponType w) {
    switch (w) {
        case WEAPON_PISTOL:   return "Pistol";
        case WEAPON_SWORD:    return "Sword";
        case WEAPON_GRENADE:  return "Grenade";
        case WEAPON_LAUNCHER: return "Launcher";
        default:              return "?";
    }
}