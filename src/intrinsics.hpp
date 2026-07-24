#pragma once

#include "globals.hpp"

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace hm {
    // LATER: использовать интринсики (для int и float оптимизации будут разными)
    template <typename T>
    static constexpr T min(T a, T b) { return a < b ? a : b; }
    template <typename T>
    static constexpr T max(T a, T b) { return a > b ? a : b; }
    template <typename T>
    __forceinline
    static constexpr T sign(T x)     { return cast<T>((x > 0) - (x < 0)); }
    template <typename T>
    static constexpr T abs(T x)      { return sign(x) * x; }

    static constexpr i32 ceil(f32 x) {
        // _mm_set_ss     // создает вектор f32
        // _mm_ceil_ss    // округляет вверх и возвращиет вектор f32
        // _mm_cvtss_si32 // округляет к ближайшему и возвращиет скаляр i32
        i32 x_trunc = cast<i32>(x);
        return x_trunc + (x_trunc < x);
    }

    static constexpr i32 floor(f32 x) {
        // _mm_set_ss     // создает вектор f32
        // _mm_floor_ss   // округляет вниз и возвращиет вектор f32
        // _mm_cvtss_si32 // округляет к ближайшему и возвращиет скаляр i32
        i32 x_trunc = cast<i32>(x);
        return x_trunc - (x_trunc > x);
    }

    template <typename Out = i32>
    __forceinline
    static constexpr Out round(f32 x) {
        // _mm_set_ss     // создает вектор f32
        // _mm_cvtss_si32 // округляет к ближайшему и возвращиет скаляр i32
        // или _mm_cvtss_f32  // округляет к ближайшему и возвращиет скаляр f32
        return cast<Out>(cast<i32>(x + 0.5f * sign(x)));
    }

    static void memzero(slice1<u8> dest) {
        // _mm_setzero_si128 + _mm_storeu_si128
        // или __stosb если больше 256 байт
        for (u8& byte : dest) byte = 0;
    }

    static void memcpy(slice1<u8> dest, slice1<const u8> src) {
        // _mm_loadu_si128 + _mm_storeu_si128
        // или __movsb если больше 256 байт
        assert(src.count <= dest.count);
        assert_no_overlap(dest, src);
        for (i64 i = 0; i < dest.count; ++i) {
            dest(i) = src(i);
        }
    }

    static result<i32> bit_scan_forward(u32 value) {
        #if defined(_MSC_VER)
            result<i32> result = {};
            result.ok = _BitScanForward(cast<unsigned long *>(&result.value), value);
            return result;
        #else
            for (i32 i = 0; i < size_of(value) * 8; ++i) {
                if (value & (1 << i)) {
                    return { true, i };
                }
            }
            return {};
        #endif
    }
}