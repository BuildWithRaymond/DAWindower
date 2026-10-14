#include "geometry.h"
#include <algorithm>

namespace daw {
Viewport viewport(int width, int height) noexcept {
    if (width <= 0 || height <= 0)
        return {};
    // Like dawnd's source-frame -> client-rectangle blit: each axis scales
    // independently. Native window resizing stays continuous and has no bars.
    return {0, 0, width, height};
}

GamePoint toGame(Viewport v, int x, int y) noexcept {
    if (v.width <= 0 || v.height <= 0)
        return {};
    // Floor negative coordinates too: outside-client input must stay outside.
    auto scale = [](std::int64_t n, int d) {
        return static_cast<int>(n >= 0 ? n / d : -((-n + d - 1) / d));
    };
    return {scale((std::int64_t(x) - v.x) * gameWidth, v.width),
            scale((std::int64_t(y) - v.y) * gameHeight, v.height),
            x >= v.x && y >= v.y && std::int64_t(x) < std::int64_t(v.x) + v.width &&
                std::int64_t(y) < std::int64_t(v.y) + v.height};
}
} // namespace daw
