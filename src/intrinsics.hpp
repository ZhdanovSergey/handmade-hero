#pragma once

#include "globals.hpp"
#include <cstring>
#include <intrin.h>

#if defined(_MSC_VER)
    // round недоступна в качестве подставляемой функции,
    // ceil и floor доступны, но не заменяются интринсиками (not true intrinsic form)
    #pragma intrinsic(abs, memcpy, memset, strcat, strlen)
    static constexpr bool MSVC_COMPILER = true;
#else
    static constexpr bool MSVC_COMPILER = false;
#endif

namespace hm {
    template <typename T>
    static constexpr T min(T a, T b) { return a < b ? a : b; }
    template <typename T>
    static constexpr T max(T a, T b) { return a > b ? a : b; }
    template <typename T>
    static constexpr T sign(T x)     { return cast<T>((x > 0) - (x < 0)); }

    static constexpr i32 ceil(f32 x) {
        i32 x_trunc = cast<i32>(x);
        return x_trunc + (x_trunc < x);
    }

    static constexpr i32 floor(f32 x) {
        i32 x_trunc = cast<i32>(x);
        return x_trunc - (x_trunc > x);
    }

    template <typename Out = i32>
    static constexpr Out round(f32 x) {
        return cast<Out>(cast<i32>(x + 0.5f * sign(x)));
    }
    
    template <typename Out = i32>
    __forceinline // draw_pixels
    static constexpr Out round_positive(f32 x) {
        assert(x >= 0);
        return cast<Out>(cast<i32>(x + 0.5f));
    }

    template <typename T>
    static constexpr T abs(T x) {
        if constexpr (MSVC_COMPILER) {
            auto result = std::abs(x);
            return cast<T>(result);
        } else {
            return sign(x) * x;
        }
    }

    static i32 strlen(const char* string) {
        if constexpr (MSVC_COMPILER) {
            auto result = std::strlen(string);
            return cast<i32>(result);
        } else {
            i32 count = 0;
            while (string[count]) count += 1;
            return count;
        }
    }

    static void strcat(char* dst, i64 dst_size, const char* src) {
        assert(strlen(dst) + strlen(src) + 1 <= dst_size);

        if constexpr (MSVC_COMPILER) {
            std::strcat(dst, src);
        } else {
            while (*dst) {
                dst += 1;
            }
            while (*src)  {
                *dst = *src;
                dst += 1;
                src  += 1;
            }
            *dst = 0;
        }
    }

    static void memzero(slice1<u8> dst) {
        if constexpr (MSVC_COMPILER) {
            size_t size = cast<size_t>(dst.get_size());
            std::memset(dst.base, 0, size);
        } else {
            for (u8& byte : dst) byte = 0;
        }
    }

    static void memcpy(void* dst, const void* src, size_t size) {
        if constexpr (MSVC_COMPILER) {
            std::memcpy(dst, src, size);
        } else {
            for (size_t i = 0; i < size; ++i) {
                cast<u8*>(dst)[i] = cast<u8*>(src)[i];
            }
        }
    }

    // static void memcpy(slice1<u8> dst, slice1<const u8> src) {
    //     assert_no_overlap(dst, src);
    //     assert(dst.count >= src.count);

    //     if constexpr (MSVC_COMPILER) {
    //         size_t size = cast<size_t>(src.get_size());
    //         std::memcpy(dst.base, src.base, size);
    //     } else {
    //         for (i64 i = 0; i < src.count; ++i) {
    //             dst(i) = src(i);
    //         }
    //     }
    // }

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