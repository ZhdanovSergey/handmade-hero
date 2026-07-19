#pragma once

#include "globals.hpp"

#if defined(_MSC_VER)
    #include <intrin.h>
    static constexpr bool MSVC_COMPILER = true;
#else
    static constexpr bool MSVC_COMPILER = false;
#endif

// TODO: добавить интринсики везде
// TODO: force inline интринсики
namespace hm {
    template <typename T>
    static constexpr T   min(T a,   T b)   { return a < b ? a : b; }
    static constexpr i32 max(i32 a, i32 b) { return a > b ? a : b; }
    template <typename T>
    static constexpr T   sign  (T x)   { return cast<T, IGNORE_ALL>((x > 0) - (x < 0)); }
    static constexpr i32 abs   (i32 x) { return sign(x) * x; }
    static constexpr i32 ceil  (f32 x) { i32 x_trunc = cast<i32, IGNORE_ALL>(x); return x_trunc + (x_trunc < x); }
    static constexpr i32 floor (f32 x) { i32 x_trunc = cast<i32, IGNORE_ALL>(x); return x_trunc - (x_trunc > x); }
    template <typename Out = i32>
    static constexpr Out round (f32 x) { return cast<Out, IGNORE_ALL>(cast<i32, IGNORE_ALL>(x + 0.5f * sign(x))); }
    template <typename Out = i32>
    static constexpr Out round_positive(f32 x) {
        assert(x >= 0);
        // TODO: заменить round на round_positive где возможно
        return cast<Out, IGNORE_ALL>(cast<i32, IGNORE_ALL>(x + 0.5f));
    }

    static void memzero(slice1<u8> dest) {
        for (u8& byte : dest) byte = 0;
    }

    static void memcpy(slice1<u8> dest, slice1<const u8> src) {
        assert(src.count <= dest.count);
        for (i64 i = 0; i < min(src.count, dest.count); ++i) {
            dest(i) = src(i);
        }
    }

    static result<i32> bit_scan_forward(u32 value) {
        if constexpr (MSVC_COMPILER) {
            result<i32> result = {};
            result.ok = _BitScanForward(cast<unsigned long *>(&result.value), value);
            return result;
        } else {
            for (i32 i = 0; i < size_of(value) * 8; ++i) {
                if (value & (1 << i)) {
                    return { true, i };
                }
            }
            return {};
        }
    }
}