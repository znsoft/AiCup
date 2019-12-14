#ifndef CODESIDE_LOOTBOX_H
#define CODESIDE_LOOTBOX_H

#include "rectangle.h"


enum class ELootType {
    PISTOL = 0,
    ASSAULT_RIFLE = 1,
    ROCKET_LAUNCHER = 2,
    HEALTH_PACK = 3,
    MINE = 4,
    NONE = 5,
};
static_assert(sizeof(WEAPON_DAMAGE) / sizeof(WEAPON_DAMAGE[0]) == (int)ELootType::NONE + 1);
static_assert(sizeof(WEAPON_AIM_SPEED) / sizeof(WEAPON_AIM_SPEED[0]) == (int)ELootType::NONE + 1);
static_assert(sizeof(WEAPON_MIN_SPREAD) / sizeof(WEAPON_MIN_SPREAD[0]) == (int)ELootType::NONE + 1);

class TLootBox : public TRectangle {
public:
    ELootType type;

    explicit TLootBox(const LootBox& lootbox) : TRectangle(lootbox.position, lootbox.size.x, lootbox.size.y) {
        auto item_ptr = lootbox.item.get();
        if (auto mine = dynamic_cast<Item::Mine*>(item_ptr)) {
            type = ELootType::MINE;
        } else if (auto weapon = dynamic_cast<Item::Weapon*>(item_ptr)) {
            type = (ELootType) weapon->weaponType;
        } else {
            type = ELootType::HEALTH_PACK;
        }
    }

    TLootBox(const TLootBox& lootbox) : TRectangle(lootbox) {
        type = lootbox.type;
    }

    int getRow() const {
        return int(x2);
    }

    int getCol() const {
        return int(y2);
    }
};

#endif //CODESIDE_LOOTBOX_H