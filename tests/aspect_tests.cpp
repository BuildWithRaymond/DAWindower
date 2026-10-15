#include "aspect.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

int main() {
    using daw::AspectDriver;
    auto size = daw::aspectSize({704, 480}, AspectDriver::width);
    check(size.width == 704 && size.height == 528, "wider client becomes taller at native 4:3");
    size = daw::aspectSize({640, 600}, AspectDriver::height);
    check(size.width == 800 && size.height == 600, "taller client becomes wider at native 4:3");
    int previous = 480;
    for (int width = 640; width <= 1024; ++width) {
        size = daw::aspectSize({width, 480}, AspectDriver::width);
        check(size.width == width && size.height >= previous && size.height <= previous + 1 &&
                  daw::nativeAspect(size),
              "every width pixel remains available without a four-pixel size grid");
        previous = size.height;
    }
    previous = 640;
    for (int height = 480; height <= 800; ++height) {
        size = daw::aspectSize({640, height}, AspectDriver::height);
        check(size.height == height && size.width >= previous && size.width <= previous + 2 &&
                  daw::nativeAspect(size),
              "every height pixel remains available with bounded rounding");
        previous = size.width;
    }
    size = daw::aspectSize({1280, 720}, AspectDriver::fit);
    check(size.width == 960 && size.height == 720,
          "wide requested box shrinks to fitted 4:3 window");
    size = daw::aspectSize({641, 480}, AspectDriver::corner);
    check(size.width == 641 && size.height == 481,
          "corner follows closest native-aspect size without a dominant-axis breakpoint");
    daw::PixelSize last{640, 480};
    for (int step = 1; step <= 128; ++step) {
        size = daw::aspectSize({640 + step, 480}, AspectDriver::corner);
        check(size.width >= last.width && size.height >= last.height &&
                  size.width - last.width <= 1 && size.height - last.height <= 1,
              "horizontal corner movement is continuous and monotonic");
        last = size;
    }
    last = daw::aspectSize({800, 600}, AspectDriver::corner);
    for (int step = 1; step <= 128; ++step) {
        size = daw::aspectSize({800 + step, 600 - step}, AspectDriver::corner);
        check(std::abs(size.width - last.width) <= 1 && std::abs(size.height - last.height) <= 1,
              "mixed-direction corner motion cannot jump when dominant axis changes");
        last = size;
    }
    size = daw::aspectSize({481, 701}, AspectDriver::fit);
    check(size.width == 481 && size.height == 361,
          "portrait requested box fits without letterboxing");
    check(daw::nativeAspect({640, 480}) && daw::nativeAspect({641, 481}) &&
              daw::nativeAspect({642, 482}) && !daw::nativeAspect({640, 479}),
          "accept only nearest-pixel 4:3 rounding");
    size = daw::aspectSize({0, 480}, AspectDriver::fit);
    check(size.width == 0 && size.height == 0, "empty bounds do not produce a viewport");
    size = daw::aspectSize({-1, 480}, AspectDriver::width);
    check(size.width == 0 && size.height == 0, "negative dimensions rejected");
    const int limit = std::numeric_limits<int>::max();
    size = daw::aspectSize({limit, limit}, AspectDriver::height);
    check(size.width > 0 && size.width <= limit && size.height > 0 && size.height <= limit &&
              daw::nativeAspect(size),
          "large sizes use bounded wide arithmetic without overflow");
    std::puts("Continuous native-aspect geometry passed.");
}
