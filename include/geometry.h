#pragma once
#include <cstdint>

namespace daw {
constexpr int gameWidth = 640;
constexpr int gameHeight = 480;

struct Viewport {
    int x, y, width, height;
};

struct GamePoint {
    int x, y;
    bool inside;
};

Viewport viewport(int width, int height) noexcept;
GamePoint toGame(Viewport view, int x, int y) noexcept;
} // namespace daw
