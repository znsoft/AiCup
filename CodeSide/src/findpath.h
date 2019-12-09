#ifndef CODESIDE_FINDPATH_H
#define CODESIDE_FINDPATH_H

#include <vector>
#include <set>
#include <map>
#include <queue>
#include <memory.h>
#include <algorithm>

struct TState {
    int x;
    int y;
    int timeLeft = 0;
    bool pad = false;

    bool operator <(const TState& state) const {
        if (x != state.x) {
            return x < state.x;
        }
        if (y != state.y) {
            return y < state.y;
        }
        if (timeLeft != state.timeLeft) {
            return timeLeft < state.timeLeft;
        }
        return pad < state.pad;
    }

    bool samePos(const TState& state) const {
        return x == state.x && y == state.y;
    }

    TPoint getPoint() const {
        return TPoint(x * (1 / 6.0), y * (1 / 6.0));
    }
};

#define SZ (6 * 45)
#define INF 0x3f3f3f3f
#define D_LEFT 0
#define D_CENTER 1
#define D_RIGHT 2

const int JUMP_TICKS_COUNT = 33;
const int JUMP_PAD_TICKS_COUNT = 31; // 31.5 actually

bool isStand[SZ][SZ];
bool isValid[SZ][SZ];
bool isBound[SZ][SZ];
bool isOnLadder[SZ][SZ];
bool isBlockedMove[3][3][SZ][SZ];
bool isTouchPad[SZ][SZ];

using TDfsGoesResult = std::vector<std::pair<TState, double>>;
std::map<TState, TDfsGoesResult> dfsGoesCache;
static bool _drawMode;
std::set<TState> dfsVisitedStates;
TDfsGoesResult dfsResultBorderPoints;
bool dfsStartMode;
TState dfsTraceTarget;
std::vector<TAction> dfsTraceActResult;
std::vector<TState> dfsTraceStateResult;

class TPathFinder {
    const TSandbox* env;
    TUnit startUnit;
    TState startState;
    std::vector<std::vector<double>> dist, penalty;
    std::vector<std::vector<TState>> prev;

    static TDfsGoesResult _getCellGoesDfs(const TState& state) {
        TDfsGoesResult res;
        const auto dy = 1 + state.pad;
        for (int xDirection = -1; xDirection <= 1; xDirection++) {
            if (isValid[state.x + xDirection][state.y + dy]) {
                if (isBlockedMove[xDirection + D_CENTER][D_CENTER + 1][state.x][state.y]) {
                    continue;
                }
                TState next = {state.x + xDirection, state.y + dy, state.timeLeft - 1, state.pad};
                if (isTouchPad[next.x][next.y]) {
                    next.timeLeft = JUMP_PAD_TICKS_COUNT;
                    next.pad = true;
                }
                res.emplace_back(next, dy);
            }
        }
        return res;
    }

    static void _dfs(TState state, double dist) {
        dfsVisitedStates.insert(state);
        if (dfsStartMode || isBound[state.x][state.y] || state.pad) {
            if (!state.pad || state.timeLeft == 0 || isOnLadder[state.x][state.y]) {
                dfsResultBorderPoints.emplace_back(state, dist);
                if (_drawMode) {
                    TDrawUtil().debugPoint(state.getPoint());
                }
            }
        }
        if (state.timeLeft == 0 || (state.pad && isOnLadder[state.x][state.y])) {
            return;
        }
        for (const auto& [to, w] : _getCellGoesDfs(state)) {
            if (!dfsVisitedStates.count(to)) {
                _dfs(to, dist + w);
            }
        }
    }

    static bool _dfsTrace(TState state) {
        dfsVisitedStates.insert(state);
        if (dfsTraceTarget.samePos(state)) {
            if (!state.pad || state.timeLeft == 0 || isOnLadder[state.x][state.y]) {
                return true;
            }
        }
        if (state.timeLeft == 0) {
            return false;
        }
        for (const auto& [to, w] : _getCellGoesDfs(state)) {
            if (!dfsVisitedStates.count(to) && _dfsTrace(to)) {
                TAction action;
                action.velocity = (to.x - state.x) * UNIT_MAX_HORIZONTAL_SPEED;
                action.jump = true;
                dfsTraceActResult.push_back(action);
                dfsTraceStateResult.push_back(to);
                return true;
            }
        }
        return false;
    }

    static TDfsGoesResult _getJumpGoes(const TState& state, bool startMode) {
        dfsStartMode = startMode;

        if (!startMode) {
            auto it = dfsGoesCache.find(state);
            if (it != dfsGoesCache.end()) {
                return it->second;
            }
        }

        dfsVisitedStates.clear();
        dfsResultBorderPoints.clear();
        _dfs(state, 0);
        if (!startMode) {
            dfsGoesCache[state] = dfsResultBorderPoints;
        }
        return dfsResultBorderPoints;
    }

public:
    static void initMap() {
        TUnit unit;
        const double d = 1 / 6.0;
        for (int i = 1; i < TLevel::width - 1; i++) {
            for (int di = 0; di < 6; di++) {
                unit.x1 = i + di * d - UNIT_HALF_WIDTH;
                unit.x2 = unit.x1 + UNIT_WIDTH;
                for (int j = 1; j < TLevel::height - 1; j++) {
                    for (int dj = 0; dj < 6; dj++) {
                        unit.y1 = j + dj * d;
                        unit.y2 = unit.y1 + UNIT_HEIGHT;

                        int ii = i*6 + di;
                        int jj = j*6 + dj;
                        isValid[ii][jj] = unit.approxIdValid();
                        isStand[ii][jj] = isValid[ii][jj] && unit.approxIsStand();
                        if (di == 0 && unit.isOnLadder()) {
                            isStand[ii][jj] = false;
                        }
                        isBound[ii][jj] = (di == 3 || di == 2 || di == 4);
                        isOnLadder[ii][jj] = isStand[ii][jj] && unit.isOnLadder();
                        isTouchPad[ii][jj] = unit.isTouchJumpPad();

                        if (di == 3 && !unit.approxIsStandGround() && unit.approxIsStandGround(1 / 6.0)) {
                            isBlockedMove[D_RIGHT][D_CENTER][ii][jj] = true;
                        }
                        if (di == 3 && !unit.approxIsStandGround() && unit.approxIsStandGround(-1 / 6.0)) {
                            isBlockedMove[D_LEFT][D_CENTER][ii][jj] = true;
                        }
                        if (!unit.approxIdValid(d/2, d/2) || unit.isTouchJumpPad(d/2, d/2)) {
                            isBlockedMove[D_RIGHT][D_RIGHT][ii][jj] = true;
                        }
                        if (!unit.approxIdValid(d/2, -d/2) || unit.isTouchJumpPad(d/2, -d/2)) {
                            isBlockedMove[D_RIGHT][D_LEFT][ii][jj] = true;
                        }
                        if (!unit.approxIdValid(-d/2, d/2) || unit.isTouchJumpPad(-d/2, d/2)) {
                            isBlockedMove[D_LEFT][D_RIGHT][ii][jj] = true;
                        }
                        if (!unit.approxIdValid(-d/2, -d/2) || unit.isTouchJumpPad(-d/2, -d/2)) {
                            isBlockedMove[D_LEFT][D_LEFT][ii][jj] = true;
                        }
                    }
                }
            }
        }
    }

    TPathFinder() {
    }

    explicit TPathFinder(const TSandbox* env, const TUnit& start) {
        this->env = env;
        this->startUnit = start;
        this->startState = _getUnitState(this->startUnit);
    }

    bool findPath(const TPoint& target, std::vector<TPoint>& res, std::vector<TAction>& resAct) {
        res.clear();
        resAct.clear();
        _run();

        TState targetState = _getPointState(target);
        TState selectedTargetState;
        std::pair<int, double> minDist(INF, INF);
        for (int dx = -7; dx <= 7; dx++) {
            for (int dy = -7; dy <= 7; dy++) {
                if (targetState.x + dx > 0 && targetState.x + dx < SZ && targetState.y + dy > 0 && targetState.y + dy < SZ) {
                    auto d = dist[targetState.x + dx][targetState.y + dy];
                    if (d < INF/2) {
                        std::pair<int, double> cand(std::abs(dx) + std::abs(dy), d);
                        if (cand < minDist) {
                            minDist = cand;
                            selectedTargetState.x = targetState.x + dx;
                            selectedTargetState.y = targetState.y + dy;
                        }
                    }
                }
            }
        }
        if (minDist.second >= INF) {
            return false;
        }

        bool qwe = false;
        auto standFix = isStand[startState.x][startState.y] && !startUnit.isStand();
        for (auto s = selectedTargetState; !s.samePos(startState); s = prev[s.x][s.y]) {
            if (prev[s.x][s.y].x == -1) {
                std::cerr << "Error state\n";
                break;
            }
            std::vector<TAction> acts;
            std::vector<TState> stts;
            auto beg = prev[s.x][s.y].samePos(startState);
            if (!_getCellGoesTrace(prev[s.x][s.y], s, beg, standFix, startUnit.isPadFly(), acts, stts)) {
                qwe = true;
                //break;
            }
            resAct.insert(resAct.end(), acts.rbegin(), acts.rend());
            for (auto& ss : stts) {
                res.push_back(ss.getPoint());
            }
            resAct.insert(resAct.end(), acts.begin(), acts.end());
        }

        auto startStatePoint = startState.getPoint();
        TAction firstAction;
        auto dx = startStatePoint.x - startUnit.position().x;
        firstAction.velocity = std::min(UNIT_MAX_HORIZONTAL_SPEED, dx * TICKS_PER_SECOND);
        bool fall = isStand[startState.x][startState.y] && !startUnit.approxIsStand();
        if (resAct.size()) {
            fall |= isStand[startState.x][startState.y] && !startUnit.canJump && resAct.back().jump;
            fall |= standFix && resAct.back().jump;
        }


        if (std::abs(dx) > 1e-10 || fall) {
            firstAction.jump = (startUnit.y1 - (int)startUnit.y1 > 0.5);
            resAct.push_back(firstAction);
        }

        res.push_back(startState.getPoint());
        std::reverse(res.begin(), res.end());
        std::reverse(resAct.begin(), resAct.end());
        return true;
    }

    std::vector<TPoint> getReachableForDraw() {
        _run();

        std::vector<TPoint> res;
#if M_DRAW_REACHABILITY_X > 0 && M_DRAW_REACHABILITY_Y > 0
        for (int i = 0; i < (int) dist.size(); i += M_DRAW_REACHABILITY_X) {
            for (int j = 0; j < (int) dist[0].size(); j += M_DRAW_REACHABILITY_Y) {
                if (dist[i][j] < INF) {
                    res.push_back(TState{i, j, 0}.getPoint());
                }
            }
        }
#endif
        return res;
    }

    template<typename TVisitor>
    void traverseReachable(TUnit unit, TVisitor visitor) {
        _run();
        for (int i = 0; i < (int) dist.size(); i++) {
            for (int j = 0; j < (int) dist[0].size(); j++) {
                if (dist[i][j] < INF) {
                    TState state{i, j, 0};
                    unit.x1 = i * (1 / 6.0) - UNIT_HALF_WIDTH;
                    unit.x2 = unit.x1 + UNIT_WIDTH;
                    unit.y1 = j * (1 / 6.0);
                    unit.y2 = unit.y1 + UNIT_HEIGHT;
                    visitor(dist[i][j], unit, state);
                }
            }
        }
    }

private:
    void _run() {
        if (dist.empty()) {
            _dijkstra();
        }
    }

    TState _getPointState(const TPoint& point) {
        TState res;
        res.x = int((point.x + (0.5 / 6)) / (1.0 / 6));
        res.y = int((point.y + 1 / 60.0) / (1.0 / 6) + 1e-8);
        return res;
    }

    TState _getUnitState(const TUnit& unit) {
        TState res = _getPointState(TPoint(unit.x1 + UNIT_HALF_WIDTH, unit.y1));
        res.timeLeft = int(unit.jumpMaxTime * 60 + 1e-10);
        res.pad = unit.isPadFly();
        return res;
    }

    std::vector<std::pair<TState, double>> _getCellGoes(const TState& state) {
        std::vector<std::pair<TState, double>> res;
        if (!isTouchPad[state.x][state.y]) {
            for (int xDirection = -1; xDirection <= 1; xDirection++) {
                auto stx = state;
                stx.x += xDirection;

                // идти по платформе
                if (isStand[stx.x][stx.y] && !isBlockedMove[xDirection + D_CENTER][D_CENTER][state.x][state.y]) {
                    res.emplace_back(stx, 0.9999);
                }

                // лететь вниз
                stx.y = state.y - 1;
                if (isValid[stx.x][stx.y] && !isBlockedMove[xDirection + D_CENTER][D_CENTER - 1][state.x][state.y]) {
                    res.emplace_back(stx, 0.9999);
                }

                // лезть по лестнице вверх
                stx.y = state.y + 1;
                if (isOnLadder[state.x][state.y] && isOnLadder[stx.x][stx.y]) {
                    res.emplace_back(stx, 0.9998);
                }
            }
        }
        // прыгать
        if (isStand[state.x][state.y] && isBound[state.x][state.y]) {
            auto pad = isTouchPad[state.x][state.y];
            auto jumpGoes = _getJumpGoes({state.x, state.y, pad ? JUMP_PAD_TICKS_COUNT : JUMP_TICKS_COUNT, pad}, false);
            for (auto& [j, dist] : jumpGoes) {
                res.emplace_back(TState{j.x, j.y, 0}, dist);
            }
        }

        return res;
    }

    bool _getCellGoesTrace(const TState& state, const TState& end, bool beg, bool standFix, bool pad, std::vector<TAction>& resAct, std::vector<TState>& resState) {
        if (!isTouchPad[state.x][state.y]) {
            for (int xDirection = -1; xDirection <= 1; xDirection++) {
                auto stx = state;
                stx.x += xDirection;
                TAction act;
                act.velocity = xDirection * UNIT_MAX_HORIZONTAL_SPEED;

                // идти по платформе
                if (stx.samePos(end)) {
                    resAct.emplace_back(act);
                    resState.emplace_back(stx);
                    return true;
                }

                // лететь вниз
                stx.y = state.y - 1;
                act.jumpDown = isStand[state.x][state.y];
                if (stx.samePos(end)) {
                    resAct.emplace_back(act);
                    resState.emplace_back(stx);
                    return true;
                }

                // лезть по лестнице вверх
                stx.y = state.y + 1;
                act.jumpDown = false;
                act.jump = true;
                if (stx.samePos(end)) {
                    resAct.emplace_back(act);
                    resState.emplace_back(stx);
                    return true;
                }
            }
        }
        // прыгать

        dfsVisitedStates.clear();
        dfsTraceActResult.clear();
        dfsTraceStateResult.clear();
        dfsTraceTarget = end;
        auto pd = isTouchPad[state.x][state.y] || (pad && beg);
        if (!_dfsTrace({state.x, state.y, beg && !standFix ? state.timeLeft : (pd ? JUMP_PAD_TICKS_COUNT : JUMP_TICKS_COUNT), pd})) {
            std::cerr << "Error trace dfs\n";
            return false;
        }
        resAct = dfsTraceActResult;
        resState = dfsTraceStateResult;
        return true;
    }

    void _dijkstra() {
        dist = std::vector<std::vector<double>>(TLevel::width*6, std::vector<double>(TLevel::height*6, INF + 1.0));
        penalty = std::vector<std::vector<double>>(TLevel::width*6, std::vector<double>(TLevel::height*6, 0));
        prev = std::vector<std::vector<TState>>(TLevel::width*6, std::vector<TState>(TLevel::height*6, {-1, -1}));

        for (auto& opp : env->units) {
            if (opp.playerId == TLevel::myId) {
                continue;
            }
            auto oppCenter = opp.center();
            auto oppCenterState = _getPointState(oppCenter);
            for (int di = -40; di <= 40; di++) {
                for (int dj = -40; dj <= 40; dj++) {
                    auto st = TState{oppCenterState.x + di, oppCenterState.y + dj, 0};
                    auto pt = st.getPoint();
                    if (st.x >= 0 && st.x < dist.size() && st.y >= 0 && st.y < dist[0].size()) {
                        const double mx = 10.0;
                        penalty[st.x][st.y] = std::max(0.0, mx - pt.getDistanceTo(oppCenter));
//                        TDrawUtil().debug->draw(CustomData::Rect(Vec2Float{float(pt.x), float(pt.y)},
//                                                                 Vec2Float{0.05, 0.05},
//                                                                 ColorFloat(1, 0, 0, penalty[st.x][st.y] / mx)));
                    }
                }
            }
        }

        std::priority_queue<std::pair<double, TState>> q;
        _drawMode = env->currentTick == 96;
        for (const auto& [s, dst] : _getJumpGoes(startState, true)) {
            q.push(std::make_pair(-dst, s));
            dist[s.x][s.y] = dst;
            prev[s.x][s.y] = startState;
        }
        while (!q.empty()) {
            auto v = q.top().second;
            auto curDist = -q.top().first;
            q.pop();
            if (curDist > dist[v.x][v.y]) {
                continue;
            }

            for (const auto& t : _getCellGoes(v)) {
                auto& to = t.first;
                auto dst = t.second + penalty[to.x][to.y];
                if (dist[v.x][v.y] + dst < dist[to.x][to.y]) {
                    dist[to.x][to.y] = dist[v.x][v.y] + dst;
                    prev[to.x][to.y] = v;
                    q.push(std::make_pair(-dist[to.x][to.y], to));
                }
            }
            continue;
        }
    }
};

#endif //CODESIDE_FINDPATH_H
