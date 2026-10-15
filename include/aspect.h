#pragma once

namespace daw {
struct PixelSize {
    int width, height;
};
enum class AspectDriver { width, height, corner, fit };

// The native 4:3 relationship rounded to the nearest representable client pixel.
// Side drags follow one axis; corners project onto the nearest 4:3 size without
// switching axes mid-drag. Fit stays within a requested programmatic box.
PixelSize aspectSize(PixelSize requested, AspectDriver driver) noexcept;
bool nativeAspect(PixelSize size) noexcept;
} // namespace daw
