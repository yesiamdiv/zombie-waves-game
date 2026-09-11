#ifndef WEAPONS_H
#define WEAPONS_H

#include <stdbool.h>

/* Player weapons. The pistol is the default and is always unlocked; the rest
 * are purchased in the shop with kill points. */
typedef enum {
    WEAPON_PISTOL = 0,
    WEAPON_SWORD,
    WEAPON_GRENADE,
    WEAPON_LAUNCHER,
    WEAPON_COUNT
} WeaponType;

/* Economy / tuning constants (kill points, shop prices, pack sizes). */
#define POINTS_PER_KILL      15
#define SWORD_COST           150
#define GRENADE_PACK_COST    75
#define GRENADES_PER_PACK    5
#define LAUNCHER_COST        450
#define ROCKETS_PER_PACK     5
#define LAUNCHER_AMMO_COST   150

/* The launcher comes with a starter clip so a fresh purchase is usable. */
#define LAUNCHER_STARTER_ROCKETS 5

typedef struct {
    WeaponType current;          /* selected weapon (1-4) */
    bool unlocked[WEAPON_COUNT]; /* sword / launcher owned */
    int grenades;                /* consumable stock (drains per throw) */
    int launcher_ammo;           /* consumable rockets (drain per shot) */
    int points;                  /* currency earned from kills */
} PlayerInventory;

void weapons_inventory_init(PlayerInventory *inv);

/* Switch the selected weapon; fails (returns false) if not unlocked. */
bool weapons_select(PlayerInventory *inv, WeaponType w);

/* Award kill points (kills count). Emits a GE_POINTS event. */
void weapons_award_kill(PlayerInventory *inv, int kills);

/* Shop purchases. Return false when the player cannot afford the item (or,
 * for one-time unlocks, already owns it). */
bool weapons_buy_sword(PlayerInventory *inv);
bool weapons_buy_grenade_pack(PlayerInventory *inv);
bool weapons_buy_launcher(PlayerInventory *inv);
bool weapons_buy_launcher_ammo(PlayerInventory *inv);

const char *weapons_name(WeaponType w);

#endif