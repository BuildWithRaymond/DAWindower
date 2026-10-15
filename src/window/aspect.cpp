#include "aspect.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace daw {
bool nativeAspect(PixelSize size) noexcept {
    const auto error = std::int64_t(size.width) * 3 - std::int64_t(size.height) * 4;
    return size.width > 0 && size.height > 0 && error >= -2 && error <= 2;
}

PixelSize aspectSize(PixelSize requested, AspectDriver driver) noexcept {
    constexpr auto limit = std::numeric_limits<int>::max();
    if (driver == AspectDriver::width) {
        if (requested.width <= 0)
            return {};
        return {requested.width,
                std::max(1, static_cast<int>((std::int64_t(requested.width) * 3 + 2) / 4))};
    }
    if (driver == AspectDriver::height) {
        if (requested.height <= 0)
            return {};
        const auto height = std::min<std::int64_t>(requested.height, (std::int64_t(limit) * 3) / 4);
        return {std::max(1, static_cast<int>((height * 4 + 1) / 3)), static_cast<int>(height)};
    }
    if (requested.width <= 0 || requested.height <= 0)
        return {};
    if (nativeAspect(requested))
        return requested;
    if (driver == AspectDriver::corner) {
        // Closest point on the 4:3 line: width = 4*(4*w + 3*h)/25.
        // A continuous projection avoids switching between incompatible width
        // and height drivers as the pointer crosses a diagonal. Round once to
        // a physical width pixel, then derive its nearest native-aspect height.
        const auto width = std::min<std::int64_t>(
            limit,
            (std::int64_t(requested.width) * 16 + std::int64_t(requested.height) * 12 + 12) / 25);
        return aspectSize({static_cast<int>(width), 0}, AspectDriver::width);
    }
    return std::int64_t(requested.width) * 3 > std::int64_t(requested.height) * 4
               ? aspectSize(requested, AspectDriver::height)
               : aspectSize(requested, AspectDriver::width);
}
} // namespace daw
