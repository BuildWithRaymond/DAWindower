#include "geometry.h"
#include <cstdio>
#include <cstdlib>

static void require(bool ok, const char* name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", name);
        std::exit(1);
    }
}

int main() {
    using namespace daw;
    auto view = viewport(1280, 720);
    require(view.x == 0 && view.y == 0 && view.width == 1280 && view.height == 720,
            "wide client filled without bars");
    auto point = toGame(view, 640, 360);
    require(point.inside && point.x == 320 && point.y == 240, "scaled center");
    point = toGame(view, 1279, 719);
    require(point.inside && point.x == 639 && point.y == 479, "last source pixel");
    point = toGame(view, -1, 360);
    require(!point.inside && point.x < 0, "outside client does not become a game click");
    require(!toGame(view, 1280, 360).inside, "exclusive right boundary");
    view = viewport(1920, 1080);
    require(view.width == 1920 && view.height == 1080 && view.x == 0 && view.y == 0,
            "entire widescreen client filled");
    view = viewport(320, 300);
    require(view.width == 320 && view.height == 300 && view.y == 0, "independent downscale axes");
    view = viewport(800, 800);
    require(view.width == 800 && view.height == 800 && view.y == 0,
            "square client filled without bars");
    require(viewport(0, 0).width == 0, "minimized geometry");
    require(!toGame({}, -32768, 32767).inside, "empty viewport");
    view = viewport(1, 1);
    require(view.width == 1 && view.height == 1, "tiny window avoids division by zero");
    std::puts("PASS: viewport and mouse mapping");
}
