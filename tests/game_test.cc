#include "src/game.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace bastion;
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

void near(float actual, float expected, const char* message) {
    check(std::abs(actual - expected) < 0.001f, message);
}

Enemy enemy(int id, float progress, float hp = 100, float speed = 0) {
    return {id, EnemyKind::Runner, progress, hp, hp, speed, 0, 9};
}

Game combat() {
    Game game;
    game.phase = Phase::Wave;
    game.wave = 1;
    // Keep the fixture in combat after the enemies are killed.
    game.remaining_to_spawn = 100;
    return game;
}

void test_road() {
    check(road().front() == Cell{0, 2}, "road starts at the gate");
    check(road().back() == Cell{15, 9}, "road ends at the core");
    for (std::size_t i = 1; i < road().size(); ++i) {
        check(std::abs(road()[i].x - road()[i - 1].x) + std::abs(road()[i].z - road()[i - 1].z) == 1,
              "road cells are contiguous");
    }
    near(position(4.5f).x, 4, "movement follows corners");
    near(position(4.5f).z, 2.5f, "movement interpolates within segments");
    near(position(-1).x, 0, "negative path positions clamp to entry");
    near(position(1000).x, 15, "large path positions clamp to core");
}

void test_build_and_economy() {
    Game game;
    check(!game.build({-1, 0}, TowerKind::Pulse), "cannot build outside grid");
    check(!game.build({16, 0}, TowerKind::Pulse), "cannot build beyond right edge");
    check(!game.build({0, 12}, TowerKind::Pulse), "cannot build beyond bottom edge");
    check(game.build({0, 2}, TowerKind::Pulse).error() == BuildError::Road, "cannot block path");
    check(game.credits == 260 && game.towers.empty(), "invalid builds have no side effects");
    check(game.build({3, 3}, TowerKind::Pulse).has_value(), "valid placement succeeds");
    check(game.credits == 190, "placement deducts exact price");
    check(game.build({3, 3}, TowerKind::Frost).error() == BuildError::Occupied, "cannot overlap towers");
    check(game.upgrade({3, 3}), "first upgrade succeeds");
    check(game.credits == 120 && game.tower_at({3, 3})->level == 2, "first upgrade accounting");
    check(game.upgrade({3, 3}), "second upgrade succeeds");
    check(game.credits == 15 && game.tower_at({3, 3})->level == 3, "second upgrade accounting");
    check(!game.upgrade({3, 3}), "tower level capped at three");
    check(game.build({7, 5}, TowerKind::Mortar).error() == BuildError::Funds, "cannot overspend");
    check(game.sell({3, 3}), "tower sells");
    check(game.credits == 186 && game.towers.empty(), "sale refunds 70 percent of all investment");
    check(!game.sell({3, 3}), "cannot sell twice");
    check(!game.upgrade({3, 3}), "cannot upgrade missing tower");
}

void test_wave_lifecycle() {
    Game game;
    game.tick(0.25f);
    check(game.enemies.empty() && game.wave == 0, "planning does not simulate");
    check(game.start_wave(), "can launch from planning");
    check(!game.start_wave(), "cannot overlap waves");
    check(game.wave == 1 && game.remaining_to_spawn == 10, "first wave has ten enemies");
    game.tick(0.01f);
    check(game.enemies.size() == 1 && game.remaining_to_spawn == 9, "wave spawns over time");
    game.enemies.clear();
    game.remaining_to_spawn = 0;
    const int money = game.credits;
    game.tick(0.01f);
    check(game.phase == Phase::Planning && game.credits == money + 40, "wave completion pays bonus");
    game.tick(0.01f);
    check(game.credits == money + 40, "wave bonus is paid exactly once");
    check(game.start_wave() && game.wave == 2, "next wave advances counter");
}

void test_priority_and_range() {
    auto game = combat();
    game.build({3, 3}, TowerKind::Pulse);
    game.enemies = {enemy(101, 2), enemy(102, 4), enemy(103, 25)};
    game.tick(0.001f);
    near(game.enemies[0].health, 100, "less advanced enemy is not targeted");
    near(game.enemies[1].health, 85, "tower targets furthest enemy in range");
    near(game.enemies[2].health, 100, "out of range enemy is not hit");
    check(game.shots.size() == 1, "attack emits visual event");
    game.tick(0.10f);
    near(game.enemies[1].health, 85, "cooldown prevents attacks every frame");
}

void test_splash_and_slow() {
    auto mortar = combat();
    mortar.build({3, 3}, TowerKind::Mortar);
    mortar.enemies = {enemy(101, 3.6f), enemy(102, 4.0f), enemy(103, 1.0f)};
    mortar.tick(0.001f);
    near(mortar.enemies[0].health, 68, "mortar damages nearby enemy");
    near(mortar.enemies[1].health, 68, "mortar damages primary enemy");
    near(mortar.enemies[2].health, 100, "mortar splash has bounded radius");

    auto frost = combat();
    frost.build({3, 3}, TowerKind::Frost);
    frost.enemies = {enemy(101, 4.0f, 100, 1)};
    frost.tick(0.001f);
    check(frost.enemies[0].slow_time > 2, "frost applies slow");
    const float start = frost.enemies[0].progress;
    frost.tick(0.10f);
    near(frost.enemies[0].progress - start, 0.045f, "frost reduces movement speed to 45 percent");
}

void test_kill_rewards() {
    auto game = combat();
    game.build({3, 3}, TowerKind::Pulse);
    game.build({2, 3}, TowerKind::Pulse);
    game.enemies = {enemy(101, 4, 1)};
    const int money = game.credits;
    game.tick(0.001f);
    check(game.kills == 1 && game.credits == money + 9, "kill awards bounty once despite multiple towers");
    check(game.enemies.size() == 1, "killed enemy removed; newly spawned enemy remains");
}

void test_end_states() {
    Game lost;
    lost.start_wave();
    lost.lives = 1;
    lost.remaining_to_spawn = 0;
    lost.enemies = {enemy(101, float(road().size() - 1))};
    lost.tick(0.001f);
    check(lost.lives == 0 && lost.phase == Phase::Lost, "leaking final life loses");
    check(lost.kills == 0, "leaking does not award a kill");
    check(!lost.start_wave() && !lost.build({0, 0}, TowerKind::Pulse), "loss prevents play");
    const int credits = lost.credits;
    lost.tick(0.25f);
    check(lost.credits == credits, "terminal state is frozen");

    Game won;
    won.wave = 9;
    won.start_wave();
    won.remaining_to_spawn = 0;
    won.tick(0.001f);
    check(won.phase == Phase::Won && won.wave == 10, "clearing final wave wins");
    check(!won.start_wave(), "no eleventh wave");

    Game boss;
    boss.start_wave();
    boss.remaining_to_spawn = 0;
    boss.enemies = {enemy(101, float(road().size() - 1))};
    boss.enemies[0].kind = EnemyKind::Titan;
    boss.tick(0.001f);
    check(boss.lives == 15, "titan leak costs five lives");
}

void test_time_validation() {
    Game game;
    game.start_wave();
    game.tick(-1);
    game.tick(0);
    game.tick(std::numeric_limits<float>::quiet_NaN());
    game.tick(std::numeric_limits<float>::infinity());
    check(game.enemies.empty(), "invalid time deltas cannot alter simulation");
    game.tick(1000);
    check(game.enemies.size() == 1 && game.enemies[0].progress < 0.5f, "long stalls are bounded");
}

void test_undefended_game() {
    Game game;
    game.start_wave();
    for (int tick = 0; tick < 30000 && game.phase != Phase::Lost; ++tick) {
        if (game.phase == Phase::Planning) game.start_wave();
        game.tick(1.0f / 60.0f);
    }
    check(game.phase == Phase::Lost, "real undefended waves eventually end in loss");
    check(game.lives == 0 && game.kills == 0, "undefended game loses through leaks");
}

void test_complete_campaign() {
    // A reproducible strategy using only starting funds, bounties, and wave bonuses.
    // This catches progression/balance regressions without bypassing combat or economy.
    struct Blueprint { Cell cell; TowerKind kind; };
    constexpr std::array<Blueprint, 12> plan{{
        {{3, 3}, TowerKind::Pulse}, {{5, 6}, TowerKind::Mortar}, {{8, 6}, TowerKind::Pulse},
        {{5, 5}, TowerKind::Frost}, {{8, 4}, TowerKind::Mortar}, {{10, 5}, TowerKind::Mortar},
        {{12, 8}, TowerKind::Pulse}, {{10, 4}, TowerKind::Frost}, {{12, 6}, TowerKind::Mortar},
        {{3, 5}, TowerKind::Pulse}, {{7, 6}, TowerKind::Mortar}, {{12, 4}, TowerKind::Pulse},
    }};
    Game game;
    for (int tick = 0; tick < 60000 && game.phase != Phase::Won && game.phase != Phase::Lost; ++tick) {
        if (game.phase == Phase::Planning) {
            for (const auto& blueprint : plan) {
                if (!game.tower_at(blueprint.cell) && !game.build(blueprint.cell, blueprint.kind)) break;
            }
            if (game.towers.size() >= 6) {
                for (int level = 1; level < 3; ++level) {
                    for (const auto& blueprint : plan) {
                        if (const auto* tower = game.tower_at(blueprint.cell); tower && tower->level == level)
                            game.upgrade(blueprint.cell);
                    }
                }
            }
            game.start_wave();
        }
        game.tick(1.0f / 30.0f);
    }
    check(game.phase == Phase::Won, "campaign is winnable with a legal strategy");
    check(game.wave == 10 && game.kills == 235, "campaign spawns all ten waves including titans");
    check(game.lives == 20 && game.credits >= 0, "strategy preserves the core without overspending");
}

}  // namespace

int main() {
    try {
        test_road();
        test_build_and_economy();
        test_wave_lifecycle();
        test_priority_and_range();
        test_splash_and_slow();
        test_kill_rewards();
        test_end_states();
        test_time_validation();
        test_undefended_game();
        test_complete_campaign();
        std::cout << "Passed " << checks << " simulation checks.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
