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
    auto v = viewport(1280, 720);
    require(v.x == 0 && v.y == 0 && v.width == 1280 && v.height == 720,
            "wide client filled without bars");
    auto p = toGame(v, 640, 360);
    require(p.inside && p.x == 320 && p.y == 240, "scaled center");
    p = toGame(v, 1279, 719);
    require(p.inside && p.x == 639 && p.y == 479, "last source pixel");
    p = toGame(v, -1, 360);
    require(!p.inside && p.x < 0, "outside client does not become a game click");
    require(!toGame(v, 1280, 360).inside, "exclusive right boundary");
    v = viewport(1920, 1080);
    require(v.width == 1920 && v.height == 1080 && v.x == 0 && v.y == 0,
            "entire widescreen client filled");
    v = viewport(320, 300);
    require(v.width == 320 && v.height == 300 && v.y == 0, "independent downscale axes");
    v = viewport(800, 800);
    require(v.width == 800 && v.height == 800 && v.y == 0, "square client filled without bars");
    require(viewport(0, 0).width == 0, "minimized geometry");
    require(!toGame({}, -32768, 32767).inside, "empty viewport");
    v = viewport(1, 1);
    require(v.width == 1 && v.height == 1, "tiny window avoids division by zero");
    std::puts("PASS: viewport and mouse mapping");
}
