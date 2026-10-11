#include "src/game.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace bastion {

const std::vector<Cell>& road() {
    static const auto cells = [] {
        constexpr std::array<Cell, 8> corners{{
            {0, 2}, {4, 2}, {4, 7}, {9, 7}, {9, 3}, {13, 3}, {13, 9}, {15, 9},
        }};
        std::vector<Cell> result{corners.front()};
        for (std::size_t i = 1; i < corners.size(); ++i) {
            auto current = result.back();
            const auto target = corners[i];
            while (current != target) {
                current.x += (target.x > current.x) - (target.x < current.x);
                current.z += (target.z > current.z) - (target.z < current.z);
                result.push_back(current);
            }
        }
        return result;
    }();
    return cells;
}

bool is_road(Cell cell) { return std::ranges::find(road(), cell) != road().end(); }

Point position(float progress) {
    const auto& cells = road();
    progress = std::clamp(progress, 0.0f, static_cast<float>(cells.size() - 1));
    const auto index = static_cast<std::size_t>(progress);
    const auto a = cells[index];
    const auto b = cells[std::min(index + 1, cells.size() - 1)];
    const float t = progress - static_cast<float>(index);
    return {a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t};
}

float distance(Point a, Point b) { return std::hypot(a.x - b.x, a.z - b.z); }
const TowerSpec& spec(TowerKind kind) { return kTowerSpecs[std::to_underlying(kind)]; }
float range(const Tower& tower) { return spec(tower.kind).range + 0.35f * (tower.level - 1); }
float damage(const Tower& tower) { return spec(tower.kind).damage * (1 + 0.65f * (tower.level - 1)); }
int upgrade_cost(const Tower& tower) { return spec(tower.kind).price * (tower.level + 1) / 2; }
int sell_value(const Tower& tower) { return tower.invested * 7 / 10; }

std::string_view error_message(BuildError error) {
    switch (error) {
        case BuildError::Outside: return "Choose a tile on the island.";
        case BuildError::Road: return "Keep the enemy route clear.";
        case BuildError::Occupied: return "A tower already occupies this tile.";
        case BuildError::Funds: return "Not enough credits. Defeat enemies to earn more.";
        case BuildError::Finished: return "Press R to begin a new defense.";
    }
    return "Cannot build here.";
}

Tower* Game::tower_at(Cell cell) {
    auto it = std::ranges::find(towers, cell, &Tower::cell);
    return it == towers.end() ? nullptr : &*it;
}

const Tower* Game::tower_at(Cell cell) const {
    auto it = std::ranges::find(towers, cell, &Tower::cell);
    return it == towers.end() ? nullptr : &*it;
}

std::expected<void, BuildError> Game::can_build(Cell cell, TowerKind kind) const {
    if (phase == Phase::Won || phase == Phase::Lost) return std::unexpected(BuildError::Finished);
    if (cell.x < 0 || cell.x >= kWidth || cell.z < 0 || cell.z >= kHeight)
        return std::unexpected(BuildError::Outside);
    if (is_road(cell)) return std::unexpected(BuildError::Road);
    if (tower_at(cell)) return std::unexpected(BuildError::Occupied);
    if (credits < spec(kind).price) return std::unexpected(BuildError::Funds);
    return {};
}

std::expected<void, BuildError> Game::build(Cell cell, TowerKind kind) {
    if (auto result = can_build(cell, kind); !result) return result;
    credits -= spec(kind).price;
    towers.push_back({cell, kind, 1, spec(kind).price, 0, {float(cell.x), float(cell.z - 1)}});
    return {};
}

bool Game::upgrade(Cell cell) {
    auto* tower = tower_at(cell);
    if (!tower || tower->level >= 3 || phase == Phase::Won || phase == Phase::Lost) return false;
    const int cost = upgrade_cost(*tower);
    if (credits < cost) return false;
    credits -= cost;
    tower->invested += cost;
    ++tower->level;
    return true;
}

bool Game::sell(Cell cell) {
    const auto* tower = tower_at(cell);
    if (!tower || phase == Phase::Won || phase == Phase::Lost) return false;
    credits += sell_value(*tower);
    std::erase_if(towers, [cell](const Tower& t) { return t.cell == cell; });
    return true;
}

bool Game::start_wave() {
    if (phase != Phase::Planning) return false;
    ++wave;
    phase = Phase::Wave;
    remaining_to_spawn = 7 + wave * 3;
    spawned_ = 0;
    spawn_clock_ = 0;
    return true;
}

void Game::spawn() {
    EnemyKind kind = EnemyKind::Runner;
    if (wave >= 2 && spawned_ % 4 == 2) kind = EnemyKind::Swarm;
    if (wave >= 3 && spawned_ % 5 == 4) kind = EnemyKind::Brute;
    if (wave >= 4 && spawned_ % 7 == 5) kind = EnemyKind::Sentinel;
    if (wave % 5 == 0 && remaining_to_spawn == 1) kind = EnemyKind::Titan;
    const float base_health = 27.0f + wave * 11.0f + wave * wave * 1.4f;
    float multiplier = 1.0f;
    float speed = 1.15f + wave * 0.045f;
    int bounty = 9;
    if (kind == EnemyKind::Swarm) { multiplier = 0.55f; speed *= 1.7f; bounty = 7; }
    if (kind == EnemyKind::Brute) { multiplier = 2.7f; speed *= 0.68f; bounty = 20; }
    if (kind == EnemyKind::Titan) { multiplier = 9.0f; speed *= 0.48f; bounty = 85; }
    if (kind == EnemyKind::Sentinel) { multiplier = 1.2f; speed *= 0.8f; bounty = 18; }
    const float hp = base_health * multiplier;
    enemies.push_back({next_id_++, kind, 0, hp, hp, speed, 0, bounty});
    --remaining_to_spawn;
    ++spawned_;
}

void Game::tick(float dt) {
    if (!std::isfinite(dt) || dt <= 0 || phase != Phase::Wave) return;
    // Bound caller stalls; substeps keep fast-forward movement and targeting stable.
    dt = std::min(dt, 0.25f);
    while (dt > 0.000001f && phase == Phase::Wave) {
        const float step_time = std::min(dt, 1.0f / 120.0f);
        step(step_time);
        dt -= step_time;
    }
}

void Game::step(float dt) {
    for (auto& shot : shots) shot.ttl -= dt;
    std::erase_if(shots, [](const Shot& s) { return s.ttl <= 0; });

    spawn_clock_ -= dt;
    if (remaining_to_spawn > 0 && spawn_clock_ <= 0) {
        spawn();
        spawn_clock_ += std::max(0.36f, 0.95f - wave * 0.045f);
    }
    for (auto& enemy : enemies) {
        const float slow = enemy.slow_time > 0 ? 0.45f : 1.0f;
        enemy.slow_time = std::max(0.0f, enemy.slow_time - dt);
        enemy.progress += enemy.speed * slow * dt;
    }

    // Leaks resolve before attacks: enemies already inside the core cannot be hit.
    std::erase_if(enemies, [this](const Enemy& enemy) {
        if (enemy.progress < static_cast<float>(road().size() - 1)) return false;
        lives = std::max(0, lives - (enemy.kind == EnemyKind::Titan ? 5 : 1));
        return true;
    });
    if (lives == 0) { phase = Phase::Lost; shots.clear(); return; }

    for (auto& tower : towers) {
        tower.cooldown = std::max(0.0f, tower.cooldown - dt);
        const Point origin{float(tower.cell.x), float(tower.cell.z)};
        Enemy* target = nullptr;
        for (auto& enemy : enemies) {
            if (enemy.health > 0 && distance(origin, position(enemy.progress)) <= range(tower) &&
                (!target || enemy.progress > target->progress)) target = &enemy;
        }
        if (!target) continue;
        tower.aim = position(target->progress);
        if (tower.cooldown > 0) continue;
        shots.push_back({origin, tower.aim, tower.kind});
        tower.cooldown = spec(tower.kind).cooldown / (1 + 0.12f * (tower.level - 1));
        const auto hit = [&](Enemy& enemy) {
            const float armor = enemy.kind == EnemyKind::Sentinel && tower.kind != TowerKind::Rail ? 0.5f : 1.0f;
            enemy.health -= damage(tower) * armor;
        };
        if (tower.kind == TowerKind::Mortar) {
            for (auto& enemy : enemies) {
                if (distance(tower.aim, position(enemy.progress)) <= 1.25f) hit(enemy);
            }
        } else {
            hit(*target);
            if (tower.kind == TowerKind::Frost) target->slow_time = 2.2f;
        }
    }
    std::erase_if(enemies, [this](const Enemy& enemy) {
        if (enemy.health > 0) return false;
        credits += enemy.bounty;
        ++kills;
        return true;
    });
    if (enemies.empty() && remaining_to_spawn == 0) {
        credits += 35 + wave * 5;
        phase = wave == kWaves ? Phase::Won : Phase::Planning;
        shots.clear();
    }
}

}  // namespace bastion
