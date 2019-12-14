#ifndef CODESIDE_BULLET_H
#define CODESIDE_BULLET_H

#include "rectangle.h"

class TBullet : public TRectangle {
public:
    ELootType weaponType;
    int unitId;
    TPoint velocity;

    TBullet() : TRectangle(0, 0, 0, 0) {
        weaponType = ELootType::NONE;
        unitId = -1;
    }

    explicit TBullet(const Bullet& bullet) : TRectangle(bullet.position.x - bullet.size / 2,
                                                        bullet.position.y - bullet.size / 2,
                                                        bullet.position.x + bullet.size / 2,
                                                        bullet.position.y + bullet.size / 2) {
        weaponType = (ELootType) bullet.weaponType;
        unitId = bullet.unitId;
        velocity.x = bullet.velocity.x;
        velocity.y = bullet.velocity.y;
    }

    TBullet(const TBullet& bullet) : TRectangle(bullet) {
        weaponType = bullet.weaponType;
        unitId = bullet.unitId;
        velocity.x = bullet.velocity.x;
        velocity.y = bullet.velocity.y;
    }

    int damage() const {
        return WEAPON_DAMAGE[(int) weaponType];
    }

    int playerId() const {
        return TLevel::unitIdToPlayerId[unitId];
    }

    bool isInWall() const {
        for (int i = int(x1); i <= int(x2); i++) {
            for (int j = int(y1); j <= int(y2); j++) {
                if (getTile(i, j) == ETile::WALL) {
                    return true;
                }
            }
        }
        return false;
    }
};

#endif //CODESIDE_BULLET_H