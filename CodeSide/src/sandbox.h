#ifndef CODESIDE_SANDBOX_H
#define CODESIDE_SANDBOX_H

#define FUCKING_EPS 1e-9

#include "model/unit.h"
#include "model/mine.h"
#include "model/lootbox.h"
#include "model/bullet.h"

#include <algorithm>
#include <iostream>
#include <tuple>

class TSandbox {
public:
    int currentTick;
    int score[2] = {0, 0};
    std::vector<TUnit> units;
    std::vector<TBullet> bullets;
    std::vector<TMine> mines;
    std::vector<TLootBox> lootBoxes;
    std::shared_ptr<std::vector<std::vector<unsigned>>> lootBoxIndex;

    TSandbox() {
        currentTick = -1;
    }

    TSandbox(const TUnit& unit, const Game& game) {
        currentTick = game.currentTick;
        for (const Unit& u : game.units) {
            units.emplace_back(u);
        }
        for (const Bullet& b : game.bullets) {
            bullets.emplace_back(b);
        }
        for (const Mine& m : game.mines) {
            mines.emplace_back(m);
        }
        std::sort(units.begin(), units.end(), [](const TUnit& a, const TUnit& b) {
            return a.id < b.id;
        });
        lootBoxIndex = std::make_shared<std::vector<std::vector<unsigned>>>(TLevel::width, std::vector<unsigned>(TLevel::height, UINT32_MAX));
        for (const LootBox& l : game.lootBoxes) {
            TLootBox lb(l);
            (*lootBoxIndex)[lb.getRow()][lb.getCol()] = (unsigned) lootBoxes.size();
            lootBoxes.emplace_back(lb);
        }
        for (const auto& player : game.players) {
            score[player.id != TLevel::myId] = player.score;
        }
    }

    TSandbox(const TSandbox& sandbox) {
        currentTick = sandbox.currentTick;
        score[0] = sandbox.score[0];
        score[1] = sandbox.score[1];
        units = sandbox.units;
        bullets = sandbox.bullets;
        mines = sandbox.mines;
        lootBoxes = sandbox.lootBoxes;
        lootBoxIndex = sandbox.lootBoxIndex;
    }


    void doTick(int updatesPerTick = UPDATES_PER_TICK) {
        for (auto& unit : units) {
            if (unit.mines > 0 && unit.action.plantMine && unit.canJump && unit.isStandOnGround()) { // TODO: check is stand
                mines.emplace_back(unit.plantMine());
            }
        }
        for (int microTick = 0; microTick < updatesPerTick; microTick++) {
            _doMicroTick(updatesPerTick);
        }
        _doMinesTick(updatesPerTick);
        currentTick++;
    }

    TLootBox* findLootBox(const TUnit& unit) {
        for (int i = int(unit.x1); i <= int(unit.x2); i++) {
            for (int j = int(unit.y1 + 1e-8); j <= int(unit.y2 + 1e-8); j++) {
                unsigned idx = (*lootBoxIndex)[i][j];
                if (idx != UINT32_MAX && lootBoxes[idx].type != ELootType::NONE && lootBoxes[idx].intersectsWith(unit)) {
                    return &lootBoxes[idx];
                }
            }
        }
        return nullptr;
    }

    TUnit* getUnit(int id) {
        for (auto& unit : units) {
            if (unit.id == id) {
                return &unit;
            }
        }
        return nullptr;
    }

private:
    void _blowUpMine(const TMine& mine) {
        for (auto& unit : units) {
            if (mine.isTouch(unit)) {
                unit.health -= MINE_EXPLOSION_DAMAGE;
                if (mine.playerId != unit.playerId) {
                    score[mine.playerId != TLevel::myId] += MINE_EXPLOSION_DAMAGE;
                }
            }
        }
        for (auto& otherMine : mines) {
            if (otherMine.state != EXPLODED && mine.isTouch(otherMine)) {
                otherMine.state = EXPLODED;
                otherMine.timer = -1;
            }
        }
    }

    void _doMinesTick(int updatesPerTick) {
        for (int i = 0; i < (int) mines.size(); i++) {
            auto& mine = mines[i];
            if (mine.state == PREPARING) {
                mine.timer -= 1.0 / (updatesPerTick * TICKS_PER_SECOND);
                if (mine.timer < -1e-10) {
                    mine.timer = -1;
                    mine.state = IDLE;
                }
            } else if (mine.state == IDLE) {
                bool trigger = false;
                for (auto& unit : units) {
                    if (mine.isTriggerOn(unit)) {
                        trigger = true;
                        break;
                    }
                }
                if (trigger) {
                    mine.state = TRIGGERED;
                    mine.timer = MINE_TRIGGER_TIME;
                }
            } else if (mine.state == TRIGGERED) {
                mine.timer -= 1.0 / (updatesPerTick * TICKS_PER_SECOND);
                if (mine.timer < -1e-10) {
                    mine.state = EXPLODED;
                }
            }

            if (mine.state == EXPLODED) {
                _blowUpMine(mine);
                mines.erase(mines.begin() + i);
                i--;
            }
        }
    }

    void _doMicrotickWeapon(TUnit& unit, double updatesPerSecond) {
        auto& weapon = unit.weapon;
        if (weapon.type == ELootType::NONE) {
            weapon.fireTimer = -1;
            weapon.lastAngle = DEFAULT_LAST_ANGLE;
            return;
        }
        if (unit.action.aim.length2() >= SQR(0.5)) {
            auto newAngle = unit.action.aim.getAngle();
            if (weapon.lastAngle < 100) {
                weapon.spread = std::min(weapon.spread + std::abs(newAngle - weapon.lastAngle), WEAPON_MAX_SPREAD);
            }
            weapon.lastAngle = newAngle;
            if (unit.action.shoot && weapon.magazine > 0 && weapon.fireTimer < -0.5) {
                bullets.emplace_back(unit.shot());
                weapon.lastFireTick = currentTick;
            }
        }
        weapon.spread = std::max(weapon.spread - WEAPON_AIM_SPEED[(int) weapon.type] / updatesPerSecond, WEAPON_MIN_SPREAD[(int) weapon.type]);
        if (weapon.fireTimer > -0.5) {
            weapon.fireTimer -= WEAPON_RELOAD_TIME / updatesPerSecond;
            if (weapon.fireTimer < 0) {
                weapon.fireTimer = -1;
            }
        }
    }

    void _doMicroTick(int updatesPerTick) {
        auto updatesPerSecond = updatesPerTick * TICKS_PER_SECOND;

        for (auto& unit : units) {
            _doMicrotickWeapon(unit, updatesPerSecond);

            for (int i = int(unit.x1); i <= int(unit.x2); i++) {
                for (int j = int(unit.y1 + 1e-8); j <= int(unit.y2 + 1e-8); j++) {
                    unsigned idx = (*lootBoxIndex)[i][j];
                    if (idx != UINT32_MAX) {
                        unit.maybeApplyLoot(lootBoxes[idx]);
                    }
                }
            }

            const auto& action = unit.action;
            auto onLadder = unit.isOnLadder();
            double dx = action.velocity / updatesPerSecond;

            if (action.velocity > 0) {
                int xcell = int(unit.x2 + dx);
                if (TLevel::tiles[xcell][int(unit.y1)] == ETile::WALL ||
                    TLevel::tiles[xcell][int(unit.y2)] == ETile::WALL ||
                    TLevel::tiles[xcell][int(unit.y1 + UNIT_HALF_HEIGHT)] == ETile::WALL) {

                    unit.x2 = xcell - FUCKING_EPS;
                    unit.x1 = unit.x2 - UNIT_WIDTH;
                } else {
                    unit.x1 += dx;
                    unit.x2 += dx;
                }
            } else if (action.velocity < 0) {
                int xcell = int(unit.x1 + dx);
                if (TLevel::tiles[xcell][int(unit.y1)] == ETile::WALL ||
                    TLevel::tiles[xcell][int(unit.y2)] == ETile::WALL ||
                    TLevel::tiles[xcell][int(unit.y1 + UNIT_HALF_HEIGHT)] == ETile::WALL) {

                    unit.x1 = xcell + 1 + FUCKING_EPS;
                    unit.x2 = unit.x1 + UNIT_WIDTH;
                } else {
                    unit.x1 += dx;
                    unit.x2 += dx;
                }
            }

            if (unit.canJump && (action.jump || !unit.jumpCanCancel)) {
                double dy = (unit.jumpCanCancel ? UNIT_JUMP_SPEED : JUMP_PAD_JUMP_SPEED) / updatesPerSecond;

                int ycell = int(unit.y2 + dy);
                if (TLevel::tiles[int(unit.x1)][ycell] == ETile::WALL || TLevel::tiles[int(unit.x2)][ycell] == ETile::WALL) {
                    unit.jumpMaxTime = 0;
                    unit.canJump = false;
                    unit.jumpCanCancel = false;
                    unit.y2 = std::max(unit.y2, ycell - FUCKING_EPS);
                    unit.y1 = unit.y2 - UNIT_HEIGHT;
                } else {
                    unit.y1 += dy;
                    unit.y2 += dy;
                    if (unit.isOnLadder()) {
                        unit.jumpMaxTime = UNIT_JUMP_TIME;
                        unit.canJump = true;
                        unit.jumpCanCancel = true;
                    } else {
                        unit.jumpMaxTime -= 1.0 / updatesPerSecond;
                    }
                }
            } else if (action.jumpDown || !onLadder) {
                double dy = -UNIT_FALL_SPEED / updatesPerSecond;

                auto wallMask = uint32_t(ETile::WALL);
                if (!action.jumpDown) {
                    if (TLevel::getTileType(unit.x1, unit.y1) != ETile::PLATFORM && TLevel::getTileType(unit.x2, unit.y1) != ETile::PLATFORM) {
                        wallMask |= uint32_t(ETile::PLATFORM);
                    }
                    if (unit.isStandOnLadder(dy)) {
                        wallMask |= uint32_t(ETile::LADDER);
                    }
                }

                int ycell = int(unit.y1 + dy);
                if (((uint32_t)TLevel::tiles[int(unit.x1)][ycell] & wallMask) ||
                    ((uint32_t)TLevel::tiles[int(unit.x2)][ycell] & wallMask)) {

                    unit.jumpMaxTime = UNIT_JUMP_TIME;
                    unit.canJump = true;
                    unit.jumpCanCancel = true;
                    unit.y1 = std::min(unit.y1, ycell + (1 + FUCKING_EPS));
                    unit.y2 = unit.y1 + UNIT_HEIGHT;
                } else {
                    unit.y1 += dy;
                    unit.y2 += dy;
                    if (unit.isOnLadder()) {
                        unit.jumpMaxTime = UNIT_JUMP_TIME;
                        unit.canJump = true;
                        unit.jumpCanCancel = true;
                    } else {
                        unit.jumpMaxTime = 0;
                        unit.canJump = false;
                        unit.jumpCanCancel = false;
                    }
                }
            } else {
                if (unit.isOnLadder()) {
                    unit.jumpMaxTime = UNIT_JUMP_TIME;
                    unit.canJump = true;
                    unit.jumpCanCancel = true;
                } else {
                    unit.jumpMaxTime = 0;
                    unit.canJump = false;
                    unit.jumpCanCancel = false;
                }
            }

            if (unit.jumpMaxTime <= -1e-11) {
                unit.canJump = false;
                unit.jumpMaxTime = 0;
                unit.jumpCanCancel = false;
            }

            if (!unit.jumpCanCancel && unit.isOnLadder()) {
                unit.canJump = true;
                unit.jumpMaxTime = UNIT_JUMP_TIME;
                unit.jumpCanCancel = true;
            }

            for (int i = int(unit.x1); i <= int(unit.x2); i++) {
                for (int j = int(unit.y1); j <= int(unit.y2); j++) {
                    if (TLevel::tiles[i][j] == ETile::JUMP_PAD) {
                        unit.jumpCanCancel = false;
                        unit.canJump = true;
                        unit.jumpMaxTime = JUMP_PAD_JUMP_TIME;
                        goto endPadLoop;
                    }
                }
            }
            endPadLoop:;
        }

        _updateBullets(updatesPerSecond, true);
        _updateBullets(updatesPerSecond, false);
    }

    void _updateBullets(double updatesPerSecond, bool checkUnitCollisionsOnly) {
        for (int i = 0; i < (int) bullets.size(); i++) {
            auto& bullet = bullets[i];
            if (!checkUnitCollisionsOnly) {
                auto dx = bullet.velocity.x / updatesPerSecond;
                auto dy = bullet.velocity.y / updatesPerSecond;
                bullet.x1 += dx;
                bullet.x2 += dx;
                bullet.y1 += dy;
                bullet.y2 += dy;
            }
            if (_bulletOut(bullet, checkUnitCollisionsOnly)) {
                bullets.erase(bullets.begin() + i);
                i--;
            }
        }
    }

    bool _bulletOut(const TBullet& bullet, bool checkUnitCollisionsOnly) {
        if (!_bulletOutIn(bullet, checkUnitCollisionsOnly)) {
            return false;
        }
        if (bullet.weaponType == ELootType::ROCKET_LAUNCHER) {
            for (auto& unit : units) {
                double d = ROCKET_LAUNCHER_EXPLOSION_RADIUS - ROCKET_LAUNCHER_BULLET_SIZE/2;
                if (unit.intersectsWith(bullet.x1 - d, bullet.y1 - d, bullet.x2 + d, bullet.y2 + d)) {
                    unit.health -= bullet.damage();
                    if (bullet.playerId() != unit.playerId) {
                        score[unit.playerId == TLevel::myId] += bullet.damage();
                    }
                }
            }
        }
        return true;
    }

    bool _bulletOutIn(const TBullet& bullet, bool checkUnitCollisionsOnly) {
        if (checkUnitCollisionsOnly) {
            for (auto& unit : units) {
                if (unit.id != bullet.unitId && unit.intersectsWith(bullet)) {
                    if (bullet.weaponType != ELootType::ROCKET_LAUNCHER) {
                        unit.health -= bullet.damage();
                        score[unit.playerId == TLevel::myId] += bullet.damage();
                    }
                    return true;
                }
            }
        } else {
            for (int i = int(bullet.x1); i <= int(bullet.x2); i++) {
                for (int j = int(bullet.y1); j <= int(bullet.y2); j++) {
                    if (getTile(i, j) == ETile::WALL) {
                        return true;
                    }
                }
            }
        }
        return false;
    }


};

#endif //CODESIDE_SANDBOX_H
