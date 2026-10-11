#pragma once

#include <array>
#include <expected>
#include <string_view>
#include <vector>

namespace bastion {

inline constexpr int kWidth = 16;
inline constexpr int kHeight = 12;
inline constexpr int kWaves = 10;

struct Point {
    float x = 0;
    float z = 0;
};

struct Cell {
    int x = 0;
    int z = 0;
    bool operator==(const Cell&) const = default;
};

enum class TowerKind { Pulse, Mortar, Frost, Rail };
enum class EnemyKind { Runner, Brute, Swarm, Titan, Sentinel };
enum class Phase { Planning, Wave, Won, Lost };
enum class BuildError { Outside, Road, Occupied, Funds, Finished };

struct TowerSpec {
    std::string_view name;
    std::string_view description;
    int price;
    float range;
    float damage;
    float cooldown;
};

inline constexpr std::array<TowerSpec, 4> kTowerSpecs{{
    {"PULSE", "Fast, focused energy bolts", 70, 3.2f, 15.0f, 0.48f},
    {"MORTAR", "Heavy splash damage", 115, 3.8f, 32.0f, 1.5f},
    {"FROST", "Slows enemies in their tracks", 90, 2.8f, 7.0f, 0.7f},
    {"RAIL", "Long-range armor-piercing shots", 150, 5.0f, 72.0f, 2.0f},
}};

struct Tower {
    Cell cell;
    TowerKind kind;
    int level = 1;
    int invested = 0;
    float cooldown = 0;
    Point aim;
};

struct Enemy {
    int id;
    EnemyKind kind;
    float progress;
    float health;
    float max_health;
    float speed;
    float slow_time = 0;
    int bounty = 0;
};

struct Shot {
    Point from;
    Point to;
    TowerKind kind;
    float ttl = 0.18f;
};

[[nodiscard]] const std::vector<Cell>& road();
[[nodiscard]] bool is_road(Cell cell);
[[nodiscard]] Point position(float progress);
[[nodiscard]] float distance(Point a, Point b);
[[nodiscard]] const TowerSpec& spec(TowerKind kind);
[[nodiscard]] float range(const Tower& tower);
[[nodiscard]] float damage(const Tower& tower);
[[nodiscard]] int upgrade_cost(const Tower& tower);
[[nodiscard]] int sell_value(const Tower& tower);
[[nodiscard]] std::string_view error_message(BuildError error);

class Game {
public:
    int credits = 260;
    int lives = 20;
    int wave = 0;
    int kills = 0;
    int remaining_to_spawn = 0;
    Phase phase = Phase::Planning;
    std::vector<Tower> towers;
    std::vector<Enemy> enemies;
    std::vector<Shot> shots;

    [[nodiscard]] std::expected<void, BuildError> can_build(Cell cell, TowerKind kind) const;
    std::expected<void, BuildError> build(Cell cell, TowerKind kind);
    bool upgrade(Cell cell);
    bool sell(Cell cell);
    bool start_wave();
    void tick(float dt);
    [[nodiscard]] Tower* tower_at(Cell cell);
    [[nodiscard]] const Tower* tower_at(Cell cell) const;

private:
    float spawn_clock_ = 0;
    int spawned_ = 0;
    int next_id_ = 1;
    void spawn();
    void step(float dt);
};

}  // namespace bastion
