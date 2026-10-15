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
    bool inside; // Whether the original client point falls inside the viewport.
};

// Fill the client rectangle; map its coordinates into the fixed logical frame.
// Nonpositive dimensions produce an empty result. Outside points stay outside.
Viewport viewport(int width, int height) noexcept;
GamePoint toGame(Viewport view, int x, int y) noexcept;
} // namespace daw
