#include "src/game.h"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace bastion;

constexpr Color kBackground{12, 21, 30, 255};
constexpr Color kPanel{20, 33, 43, 255};
constexpr Color kCard{28, 46, 55, 255};
constexpr Color kText{233, 239, 225, 255};
constexpr Color kMuted{131, 157, 162, 255};
constexpr Color kGold{246, 194, 91, 255};
constexpr Color kMint{110, 225, 188, 255};
constexpr Color kRed{245, 112, 112, 255};
constexpr Color kBlue{117, 192, 249, 255};
constexpr std::array<Color, 3> kTowerColors{kMint, kGold, kBlue};

Vector3 world(Point point, float y = 0) { return {point.x - 7.5f, y, point.z - 5.5f}; }
Vector3 world(Cell cell, float y = 0) { return world(Point{float(cell.x), float(cell.z)}, y); }
Color tower_color(TowerKind kind) { return kTowerColors[std::to_underlying(kind)]; }

void label(std::string_view text, float x, float y, float size, Color color = kText) {
    DrawTextEx(GetFontDefault(), std::string(text).c_str(), {x, y}, size, 1.0f, color);
}

void centered(std::string_view text, float x, float y, float size, Color color = kText) {
    const std::string value(text);
    const float width = MeasureTextEx(GetFontDefault(), value.c_str(), size, 1).x;
    label(text, x - width / 2, y, size, color);
}

void card(Rectangle rect, Color color, float roundness = 0.13f) {
    DrawRectangleRounded(rect, roundness, 8, color);
}

bool button(Rectangle rect, std::string_view text, bool enabled = true, bool primary = false) {
    const bool hover = CheckCollisionPointRec(GetMousePosition(), rect);
    const Color background = !enabled ? kCard : primary ? kGold : hover ? Color{45, 69, 77, 255} : kCard;
    card(rect, background);
    centered(text, rect.x + rect.width / 2, rect.y + (rect.height - 18) / 2, 18,
             !enabled ? kMuted : primary ? kBackground : kText);
    return enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void ring(Vector3 center, float radius, Color color) {
    constexpr int segments = 80;
    for (int i = 0; i < segments; ++i) {
        const float a = i * 2 * PI / segments;
        const float b = (i + 1) * 2 * PI / segments;
        DrawLine3D({center.x + std::cos(a) * radius, center.y, center.z + std::sin(a) * radius},
                   {center.x + std::cos(b) * radius, center.y, center.z + std::sin(b) * radius}, color);
    }
}

void draw_tower(const Tower& tower, bool ghost = false) {
    const auto tint = [ghost](Color color) { return ghost ? Fade(color, 0.5f) : color; };
    const Color accent = tint(tower_color(tower.kind));
    const Vector3 p = world(tower.cell);
    DrawCylinder({p.x, 0.11f, p.z}, 0.52f, 0.52f, 0.025f, 24, tint(Color{17, 39, 40, 255}));
    DrawCube({p.x, 0.22f, p.z}, 0.73f, 0.3f, 0.73f, tint(Color{57, 78, 82, 255}));
    DrawCube({p.x, 0.39f, p.z}, 0.64f, 0.06f, 0.64f, accent);
    const float height = 0.65f + tower.level * 0.13f;
    if (tower.kind == TowerKind::Pulse) {
        DrawCylinder({p.x, 0.43f, p.z}, 0.22f, 0.28f, height - 0.3f, 8, tint(Color{72, 98, 103, 255}));
        DrawSphere({p.x, height, p.z}, 0.22f, accent);
        const Vector3 target = world(tower.aim, height);
        Vector3 dir = Vector3Normalize(Vector3Subtract(target, {p.x, height, p.z}));
        if (Vector3Length(dir) < 0.01f) dir = {0, 0, -1};
        DrawCylinderEx({p.x, height, p.z}, Vector3Add({p.x, height, p.z}, Vector3Scale(dir, 0.52f)),
                       0.10f, 0.07f, 8, tint(kText));
    } else if (tower.kind == TowerKind::Mortar) {
        DrawCylinder({p.x, 0.43f, p.z}, 0.31f, 0.36f, 0.35f, 8, tint(Color{94, 97, 85, 255}));
        Vector3 direction = Vector3Normalize(Vector3Subtract(world(tower.aim), p));
        if (Vector3Length(direction) < 0.01f) direction = {0, 0, -1};
        const auto end = Vector3Add({p.x, 1.02f, p.z}, Vector3Scale(direction, 0.35f));
        DrawCylinderEx({p.x, 0.64f, p.z}, end, 0.23f, 0.22f, 10, accent);
        DrawSphere(end, 0.15f, tint(kBackground));
    } else {
        DrawCylinder({p.x, 0.43f, p.z}, 0.16f, 0.27f, 0.38f, 6, tint(Color{76, 106, 126, 255}));
        DrawCylinder({p.x, 0.77f, p.z}, 0.0f, 0.24f, 0.48f, 4, accent);
        DrawCylinder({p.x, 0.57f, p.z}, 0.24f, 0.0f, 0.20f, 4, accent);
        ring({p.x, 0.90f, p.z}, 0.38f, accent);
    }
    for (int i = 0; i < tower.level; ++i)
        DrawCube({p.x - 0.20f + i * 0.2f, 0.26f, p.z + 0.374f}, 0.10f, 0.08f, 0.02f, accent);
}

void draw_enemy(const Enemy& enemy, float time) {
    const auto p = world(position(enemy.progress));
    const bool boss = enemy.kind == EnemyKind::Titan;
    const float scale = boss ? 0.9f : enemy.kind == EnemyKind::Brute ? 0.62f : 0.40f;
    const float bob = std::sin(time * 9 + enemy.id) * 0.035f;
    const Color color = enemy.slow_time > 0 ? kBlue : boss ? Color{216, 103, 174, 255} :
                        enemy.kind == EnemyKind::Brute ? Color{216, 133, 92, 255} : kRed;
    DrawCylinder({p.x, 0.13f, p.z}, scale * 0.65f, scale * 0.65f, 0.015f, 16, Color{94, 81, 63, 255});
    DrawCube({p.x, 0.24f + scale / 2 + bob, p.z}, scale, scale * 0.8f, scale, color);
    DrawCube({p.x, 0.29f + scale / 2 + bob, p.z + scale / 2 + 0.01f}, scale * 0.66f, 0.09f, 0.02f, kBackground);
    if (boss) {
        DrawCylinder({p.x, 1.0f + bob, p.z}, 0.1f, 0.34f, 0.4f, 4, kGold);
    } else if (enemy.kind == EnemyKind::Swarm) {
        DrawCube({p.x, 0.55f + bob, p.z}, 0.62f, 0.05f, 0.15f, color);
    }
    DrawCube({p.x - scale * 0.28f, 0.20f, p.z}, 0.11f, 0.18f, scale * 0.8f, Color{67, 52, 60, 255});
    DrawCube({p.x + scale * 0.28f, 0.20f, p.z}, 0.11f, 0.18f, scale * 0.8f, Color{67, 52, 60, 255});
}

void draw_battlefield(const Game& game, std::optional<Cell> hover, std::optional<Cell> selected,
                      TowerKind build_kind, bool building, float time) {
    DrawCube({0, -0.61f, 0}, 16.3f, 1.15f, 12.3f, Color{30, 52, 57, 255});
    DrawCube({0, -1.22f, 0}, 15.9f, 0.14f, 11.9f, Color{20, 37, 46, 255});
    for (int z = 0; z < kHeight; ++z) {
        for (int x = 0; x < kWidth; ++x) {
            const Cell cell{x, z};
            const bool path = is_road(cell);
            const int noise = (x * 17 + z * 31) % 13;
            const Color color = path ? Color{static_cast<unsigned char>(182 + noise),
                                            static_cast<unsigned char>(163 + noise), 120, 255}
                                    : Color{static_cast<unsigned char>(43 + noise / 2),
                                            static_cast<unsigned char>(83 + noise),
                                            static_cast<unsigned char>(76 + noise / 2), 255};
            DrawCube(world(cell, 0), 0.975f, path ? 0.18f : 0.12f, 0.975f, color);
            if (!path && !game.tower_at(cell) && (x * 7 + z * 11) % 17 == 0) {
                const auto p = world(cell, 0.10f);
                DrawCube({p.x - 0.27f, p.y, p.z - 0.28f}, 0.13f, 0.08f, 0.17f, Color{81, 116, 91, 255});
            }
        }
    }
    // Light strips on the exposed sides of the floating island.
    for (int i = 0; i < 8; ++i) {
        DrawCube({-7.0f + i * 2, -0.52f, 6.16f}, 0.35f, 0.055f, 0.025f, Fade(kMint, 0.7f));
        DrawCube({8.16f, -0.52f, -5.0f + i * 1.4f}, 0.025f, 0.055f, 0.30f, Fade(kMint, 0.7f));
    }
    const auto entrance = world(road().front());
    for (float dz : {-0.47f, 0.47f}) {
        DrawCube({entrance.x, 0.62f, entrance.z + dz}, 0.20f, 1.15f, 0.18f, Color{87, 61, 66, 255});
        DrawCube({entrance.x, 0.88f, entrance.z + dz}, 0.22f, 0.08f, 0.20f, kRed);
    }
    DrawCube({entrance.x, 1.24f, entrance.z}, 0.22f, 0.15f, 1.14f, kRed);
    const auto core = world(road().back());
    DrawCylinder({core.x, 0.13f, core.z}, 0.46f, 0.55f, 0.28f, 8, Color{61, 91, 99, 255});
    const float float_y = 0.84f + std::sin(time * 2) * 0.08f;
    DrawCylinder({core.x, float_y, core.z}, 0, 0.30f, 0.50f, 4, kMint);
    DrawCylinder({core.x, float_y - 0.4f, core.z}, 0.30f, 0, 0.4f, 4, Color{78, 171, 156, 255});
    ring({core.x, 0.35f, core.z}, 0.65f, kMint);
    for (const auto& tower : game.towers) draw_tower(tower);
    for (const auto& enemy : game.enemies) draw_enemy(enemy, time);
    for (const auto& shot : game.shots) {
        const float alpha = std::clamp(shot.ttl / 0.18f, 0.0f, 1.0f);
        const Color color = Fade(tower_color(shot.kind), alpha);
        DrawCylinderEx(world(shot.from, 0.95f), world(shot.to, 0.40f), 0.032f, 0.018f, 6, color);
        DrawSphere(world(shot.to, 0.40f), (1 - alpha) * 0.25f + 0.05f, color);
        if (shot.kind == TowerKind::Mortar) ring(world(shot.to, 0.17f), (1 - alpha) * 1.25f, color);
    }
    if (selected) {
        if (const auto* tower = game.tower_at(*selected)) {
            DrawCubeWires(world(*selected, 0.14f), 0.98f, 0.15f, 0.98f, kGold);
            ring(world(*selected, 0.17f), range(*tower), Fade(tower_color(tower->kind), 0.85f));
        }
    }
    if (hover && building && !game.tower_at(*hover)) {
        const bool valid = game.can_build(*hover, build_kind).has_value();
        DrawCube(world(*hover, 0.13f), 0.97f, 0.06f, 0.97f, Fade(valid ? kMint : kRed, 0.48f));
        if (valid) {
            Tower preview{*hover, build_kind, 1, 0, 0, {float(hover->x), float(hover->z - 1)}};
            draw_tower(preview, true);
            ring(world(*hover, 0.17f), spec(build_kind).range, Fade(tower_color(build_kind), 0.6f));
        }
    }
}

std::optional<Cell> pick_cell(Vector2 mouse, Camera3D camera, int width, int height) {
    const auto ray = GetScreenToWorldRayEx(mouse, camera, width, height);
    if (std::abs(ray.direction.y) < 0.0001f) return std::nullopt;
    const float t = (0.10f - ray.position.y) / ray.direction.y;
    if (t <= 0) return std::nullopt;
    const auto hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    const Cell cell{static_cast<int>(std::floor(hit.x + 8)), static_cast<int>(std::floor(hit.z + 6))};
    if (cell.x < 0 || cell.x >= kWidth || cell.z < 0 || cell.z >= kHeight) return std::nullopt;
    return cell;
}

void draw_help(int width, int height) {
    DrawRectangle(0, 0, width, height, Fade(BLACK, 0.75f));
    const float x = width / 2.0f - 275;
    const float y = height / 2.0f - 235;
    card({x, y, 550, 470}, kPanel);
    label("FIELD MANUAL", x + 30, y + 26, 28, kGold);
    label("Protect the crystal through ten waves.", x + 30, y + 70, 19);
    constexpr std::array<std::pair<std::string_view, std::string_view>, 8> controls{{
        {"1 / 2 / 3", "Select pulse / mortar / frost"},
        {"LEFT CLICK", "Build, or inspect an existing tower"},
        {"U / X", "Upgrade / sell selected tower"},
        {"SPACE", "Launch the next wave"},
        {"P / F", "Pause / toggle double speed"},
        {"Q / E / WHEEL", "Orbit / zoom the camera"},
        {"RIGHT CLICK / ESC", "Cancel build or selection"},
        {"R / H", "Restart / toggle this manual"},
    }};
    int row = 0;
    for (const auto& [key, action] : controls) {
        label(key, x + 30, y + 116 + row * 31, 16, kMint);
        label(action, x + 212, y + 116 + row * 31, 15);
        ++row;
    }
    label("Tip: put frost before a bend, then cover it with mortar.", x + 30, y + 380, 15, kMuted);
    label("The battlefield pauses while this manual is open.", x + 30, y + 407, 15, kMuted);
    centered("H or ESC to close", width / 2.0f, y + 440, 16, kGold);
}
}  // namespace

int main(int argc, char** argv) {
    bool smoke_test = false;
    std::string screenshot;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--smoke-test") smoke_test = true;
        else if (arg == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else if (arg == "--help") {
            std::puts("Bastion: bazel run //:tower_defense\nOptions: --smoke-test [--screenshot /absolute/path.png]");
            return 0;
        } else { std::fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]); return 2; }
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1440, 900, "BASTION // Last Light");
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(1120, 800);
    SetExitKey(KEY_NULL);
    SetTargetFPS(smoke_test ? 0 : 60);

    Game game;
    TowerKind build_kind = TowerKind::Pulse;
    bool building = true;
    bool paused = false;
    bool fast = false;
    bool help = false;
    std::optional<Cell> selected;
    float camera_angle = 0.78f;
    float zoom = 18.8f;
    float time = 0;
    float notice_time = 8;
    std::string notice = "Select a tower, then click a green tile. SPACE launches wave 1.";
    RenderTexture2D scene{};
    int texture_width = 0;
    int texture_height = 0;
    int frames = 0;
    bool smoke_passed = true;

    if (smoke_test) {
        if (!game.build({3, 3}, TowerKind::Pulse) ||
            !game.build({5, 6}, TowerKind::Mortar) ||
            !game.build({8, 6}, TowerKind::Pulse)) {
            std::fprintf(stderr, "Smoke test setup failed: could not place the initial towers.\n");
            CloseWindow();
            return 1;
        }
        game.start_wave();
        building = false;
        notice_time = 0;
    }

    while (!WindowShouldClose()) {
        const int width = GetScreenWidth();
        const int height = GetScreenHeight();
        const int view_width = width - 326;
        const int view_height = height - 152;
        constexpr float view_x = 16;
        constexpr float view_y = 104;
        const float side_x = width - 294.0f;
        const Rectangle viewport{view_x, view_y, float(view_width), float(view_height)};
        const float dt = smoke_test ? 1.0f / 60.0f : std::min(GetFrameTime(), 0.1f);
        time += dt;
        notice_time = std::max(0.0f, notice_time - dt);

        if (view_width != texture_width || view_height != texture_height) {
            if (scene.id != 0) UnloadRenderTexture(scene);
            scene = LoadRenderTexture(view_width, view_height);
            if (scene.id == 0) { CloseWindow(); return 1; }
            SetTextureFilter(scene.texture, TEXTURE_FILTER_BILINEAR);
            texture_width = view_width;
            texture_height = view_height;
        }
        if (IsKeyPressed(KEY_H)) help = !help;
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (help) help = false;
            else { building = false; selected.reset(); }
        }
        const auto notify = [&](std::string text) { notice = std::move(text); notice_time = 3.5f; };
        const auto launch_wave = [&] {
            if (game.start_wave()) notify("Wave " + std::to_string(game.wave) + " incoming. Protect the crystal!");
        };
        const auto restart = [&] {
            game = Game{};
            selected.reset();
            building = true;
            paused = false;
            fast = false;
            notify("Fresh defenses. Build your first towers, then launch wave 1.");
        };
        const auto upgrade = [&] {
            if (selected && !game.upgrade(*selected)) notify("Upgrade unavailable: check credits and tower level.");
        };
        const auto sell = [&] {
            if (selected && game.sell(*selected)) { selected.reset(); notify("Tower recycled. 70% of your investment returned."); }
        };

        if (!help) {
            if (IsKeyPressed(KEY_R)) restart();
            if (IsKeyPressed(KEY_P)) paused = !paused;
            if (IsKeyPressed(KEY_F)) fast = !fast;
            if (IsKeyPressed(KEY_SPACE) && !paused) launch_wave();
            if (IsKeyPressed(KEY_U)) upgrade();
            if (IsKeyPressed(KEY_X)) sell();
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { selected.reset(); building = false; }
            for (int i = 0; i < 3; ++i) {
                if (IsKeyPressed(KEY_ONE + i)) {
                    build_kind = static_cast<TowerKind>(i);
                    building = true;
                    selected.reset();
                }
            }
            if (IsKeyDown(KEY_Q)) camera_angle -= dt * 0.8f;
            if (IsKeyDown(KEY_E)) camera_angle += dt * 0.8f;
            if (CheckCollisionPointRec(GetMousePosition(), viewport))
                zoom = std::clamp(zoom - GetMouseWheelMove(), 12.0f, 27.0f);
        }

        Camera3D camera{};
        camera.position = {std::sin(camera_angle) * 24, 24, std::cos(camera_angle) * 24};
        camera.target = {0, 0, 0};
        camera.up = {0, 1, 0};
        camera.fovy = zoom;
        camera.projection = CAMERA_ORTHOGRAPHIC;

        std::optional<Cell> hover;
        const bool finished = game.phase == Phase::Won || game.phase == Phase::Lost;
        if (!help && !finished && CheckCollisionPointRec(GetMousePosition(), viewport)) {
            hover = pick_cell({GetMouseX() - view_x, GetMouseY() - view_y}, camera, view_width, view_height);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (game.tower_at(*hover)) { selected = hover; building = false; }
                else if (building) {
                    if (auto result = game.build(*hover, build_kind); !result) notify(std::string(error_message(result.error())));
                    else notify(std::string(spec(build_kind).name) + " deployed. Keep building or select a tower to upgrade.");
                } else selected.reset();
            }
        }
        const auto previous_phase = game.phase;
        if (!paused && !help) game.tick(dt * (fast ? 2.0f : 1.0f));
        if (previous_phase == Phase::Wave && game.phase == Phase::Planning)
            notify("Wave secured! Spend your bounty and press SPACE when ready.");

        BeginTextureMode(scene);
        ClearBackground(Color{17, 29, 39, 255});
        for (int i = 0; i < 65; ++i) {
            const int x = (i * 197 + 83) % view_width;
            const int y = (i * 131 + 37) % view_height;
            DrawCircle(x, y, i % 4 == 0 ? 1.5f : 1.0f, Fade(kMuted, 0.18f));
        }
        BeginMode3D(camera);
        draw_battlefield(game, hover, selected, build_kind, building && !finished, time);
        EndMode3D();
        for (const auto& enemy : game.enemies) {
            const auto p = GetWorldToScreenEx(world(position(enemy.progress), enemy.kind == EnemyKind::Titan ? 1.65f : 1.0f),
                                              camera, view_width, view_height);
            DrawRectangle(int(p.x - 15), int(p.y), 30, 4, kBackground);
            DrawRectangle(int(p.x - 15), int(p.y), int(30 * std::max(0.0f, enemy.health / enemy.max_health)), 4,
                          enemy.slow_time > 0 ? kBlue : kRed);
        }
        EndTextureMode();

        BeginDrawing();
        ClearBackground(kBackground);
        label("B A S T I O N", 28, 24, 32);
        label("L A S T   L I G H T   /   TOWER DEFENSE", 30, 65, 12, kMuted);
        const float stat_x = width - 770.0f;
        label("CREDITS", stat_x, 25, 13, kMuted);
        label(std::to_string(game.credits), stat_x, 47, 27, kGold);
        label("CORE", stat_x + 165, 25, 13, kMuted);
        label(std::to_string(game.lives) + " / 20", stat_x + 165, 47, 27, game.lives < 6 ? kRed : kMint);
        label("WAVE", stat_x + 330, 25, 13, kMuted);
        label(std::to_string(game.wave) + " / 10", stat_x + 330, 47, 27);
        if (button({width - 145.0f, 27, 116, 42}, "H  MANUAL", !help)) help = true;

        DrawTextureRec(scene.texture, {0, 0, float(view_width), -float(view_height)}, {view_x, view_y}, WHITE);
        DrawRectangleLinesEx(viewport, 1, Color{38, 56, 65, 255});
        card({view_x + 18, view_y + 18, 245, 48}, Fade(kBackground, 0.85f));
        DrawCircle(int(view_x + 37), int(view_y + 42), 4, game.phase == Phase::Wave ? kRed : kMint);
        label(game.phase == Phase::Wave ? "HOSTILES INBOUND" : "THE VERDANT CAUSEWAY", view_x + 52, view_y + 35, 15);
        label("SECTOR 07  /  HOLD THE LINE", view_x + 24, view_y + view_height - 28, 12, kMuted);
        if (notice_time > 0 && !finished) {
            card({view_x + 18, view_y + view_height - 80, float(view_width - 36), 38}, Fade(kBackground, 0.9f));
            label(notice, view_x + 32, view_y + view_height - 69, 15, kGold);
        }

        card({side_x - 8, 104, 286, float(height - 152)}, kPanel, 0.025f);
        label("BUILD YOUR DEFENSE", side_x + 10, 123, 18);
        label("Choose a blueprint. Place on grass.", side_x + 10, 152, 13, kMuted);
        for (int i = 0; i < 3; ++i) {
            const auto kind = static_cast<TowerKind>(i);
            const auto& info = spec(kind);
            const Rectangle rect{side_x + 8, 184.0f + i * 80, 254, 70};
            const bool active = building && build_kind == kind;
            const bool over = CheckCollisionPointRec(GetMousePosition(), rect);
            card(rect, active ? Color{44, 67, 69, 255} : over && !help ? Color{35, 53, 63, 255} : kCard);
            if (active) DrawRectangleRoundedLinesEx(rect, 0.13f, 8, 1.5f, tower_color(kind));
            card({rect.x + 10, rect.y + 12, 39, 44}, Fade(tower_color(kind), 0.15f));
            centered(std::to_string(i + 1), rect.x + 29, rect.y + 23, 22, tower_color(kind));
            label(info.name, rect.x + 61, rect.y + 12, 19, tower_color(kind));
            label(std::to_string(info.price), rect.x + 205, rect.y + 13, 18, game.credits >= info.price ? kGold : kMuted);
            label(i == 0 ? "QUICK FIRE / SINGLE" : i == 1 ? "AREA DAMAGE / HEAVY" : "SLOW FIELD / SUPPORT",
                  rect.x + 61, rect.y + 42, 10, kMuted);
            if (!help && !finished && over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                building = true;
                build_kind = kind;
                selected.reset();
            }
        }

        DrawLine(int(side_x + 10), 439, int(side_x + 258), 439, Color{47, 67, 75, 255});
        if (const Tower* tower = selected ? game.tower_at(*selected) : nullptr) {
            label(std::string(spec(tower->kind).name) + "  /  LEVEL " + std::to_string(tower->level), side_x + 12, 459, 19, tower_color(tower->kind));
            label("DMG " + std::to_string(int(damage(*tower))) + "    RANGE " + std::to_string(range(*tower)).substr(0, 3),
                  side_x + 12, 491, 14, kMuted);
            const bool can_upgrade = tower->level < 3 && game.credits >= upgrade_cost(*tower) && !finished && !help;
            const std::string upgrade_label = tower->level == 3 ? "MAX LEVEL" : "U  UPGRADE  /  " + std::to_string(upgrade_cost(*tower));
            const std::string sell_label = "X  SELL  /  +" + std::to_string(sell_value(*tower));
            if (button({side_x + 10, 521, 250, 40}, upgrade_label, can_upgrade, true)) upgrade();
            if (button({side_x + 10, 570, 250, 36}, sell_label, !help && !finished)) sell();
        } else {
            label("TACTICAL NOTES", side_x + 12, 459, 16, kGold);
            label("Build around bends to cover", side_x + 12, 491, 15, kMuted);
            label("more of the enemy route.", side_x + 12, 513, 15, kMuted);
            label("Frost + mortar: slow them", side_x + 12, 548, 15, kMuted);
            label("down, then hit the whole pack.", side_x + 12, 570, 15, kMuted);
            label("Click a tower to upgrade it.", side_x + 12, 606, 14, kMint);
        }

        const float control_y = height - 165.0f;
        const std::string wave_label = game.phase == Phase::Wave ? "WAVE IN PROGRESS" : "SPACE  /  LAUNCH WAVE " + std::to_string(game.wave + 1);
        if (button({side_x + 10, control_y, 250, 46}, finished ? "R  /  PLAY AGAIN" : wave_label,
                   !help && (finished || (game.phase == Phase::Planning && !paused)), true)) {
            if (finished) restart(); else launch_wave();
        }
        if (button({side_x + 10, control_y + 57, 120, 35}, paused ? "P  RESUME" : "P  PAUSE", !help && !finished)) paused = !paused;
        if (button({side_x + 140, control_y + 57, 120, 35}, fast ? "F  2x" : "F  1x", !help && !finished)) fast = !fast;

        label("1-3  BUILD     U  UPGRADE     Q/E  ORBIT     SCROLL  ZOOM", 28, height - 29.0f, 13, kMuted);
        label("ELIMINATED  " + std::to_string(game.kills), width - 280.0f, height - 29.0f, 13, kMint);
        if (game.phase == Phase::Wave) {
            label("ON ROUTE  " + std::to_string(game.enemies.size()) + "   /   INCOMING  " + std::to_string(game.remaining_to_spawn),
                  view_x + 24, view_y + 81, 13, kMuted);
        }

        if (paused && !finished && !help) {
            card({view_x + view_width / 2.0f - 130, view_y + view_height / 2.0f - 47, 260, 94}, Fade(kBackground, 0.95f));
            centered("PAUSED", view_x + view_width / 2.0f, view_y + view_height / 2.0f - 26, 29, kGold);
            centered("P to resume", view_x + view_width / 2.0f, view_y + view_height / 2.0f + 17, 16, kMuted);
        }
        if (game.phase == Phase::Won || game.phase == Phase::Lost) {
            DrawRectangleRec(viewport, Fade(kBackground, 0.78f));
            const float cx = view_x + view_width / 2.0f;
            const float cy = view_y + view_height / 2.0f;
            centered(game.phase == Phase::Won ? "THE LIGHT HOLDS" : "CORE OVERRUN", cx, cy - 72, 40,
                     game.phase == Phase::Won ? kMint : kRed);
            centered(game.phase == Phase::Won ? "Ten waves. One unbroken bastion." : "Regroup. Rebuild. Hold the next line.", cx, cy - 9, 21);
            centered(std::to_string(game.kills) + " enemies eliminated  /  " + std::to_string(game.credits) + " credits remaining", cx, cy + 33, 17, kMuted);
            if (button({cx - 120, cy + 81, 240, 46}, "R  /  TRY AGAIN", !help, true)) restart();
        }
        if (help) draw_help(width, height);
        EndDrawing();

        ++frames;
        if (smoke_test && frames == 300) {
            if (!screenshot.empty()) {
                const Image capture = LoadImageFromScreen();
                smoke_passed = ExportImage(capture, screenshot.c_str());
                UnloadImage(capture);
            }
            smoke_passed = smoke_passed && game.towers.size() == 3 && game.wave == 1 && game.kills > 0;
            std::printf("Smoke test: rendered %d frames, %zu towers, %d kills, %zu enemies.\n",
                        frames, game.towers.size(), game.kills, game.enemies.size());
            break;
        }
    }
    UnloadRenderTexture(scene);
    CloseWindow();
    return smoke_passed ? 0 : 1;
}
