#pragma once

#include "globals.hpp"
#include <cstring>
#include <intrin.h>

#if defined(_MSC_VER)
    #pragma intrinsic(abs, memcpy, memset, strcat, strlen) // ceil и floor доступны, но не заменяются интринсиками (not true intrinsic form)
    static constexpr bool MSVC_COMPILER = true;
#else
    static constexpr bool MSVC_COMPILER = false;
#endif

namespace hm {
    template <typename T>
    static T min(T a, T b) { return a < b ? a : b; }
    
    template <typename T>
    static T max(T a, T b) { return a > b ? a : b; }

    template <typename Out_Provider = void, typename In,
                typename Out = conditional_t< is_same_v<Out_Provider, void>, In, Out_Provider>>
    static Out sign(In x) { return cast<Out>((x > 0) - (x < 0)); }

    static i32 ceil(f32 x) {
        i32 x_trunc = cast<i32>(x);
        return x_trunc + (x_trunc < x);
    }

    static i32 floor(f32 x) {
        i32 x_trunc = cast<i32>(x);
        return x_trunc - (x_trunc > x);
    }

    template <typename Out = f32>
    static Out round(f32 x) {
        return cast<Out>(cast<i32>(x + 0.5f * sign(x)));
    }

    template <typename Out = f32>
    __forceinline // because of draw_pixels
    static Out round_positive(f32 x) {
        assert(x >= 0);
        return cast<Out>(cast<i32>(x + 0.5f));
    }
    
    template <typename T>
    static T abs(T x) {
        if constexpr (MSVC_COMPILER) {
            auto result = std::abs(x);
            return cast<T>(result);
        } else {
            return sign(x) * x;
        }
    }

    static i32 strlen(cstr string) {
        if constexpr (MSVC_COMPILER) {
            auto result = std::strlen(string);
            return cast<i32>(result);
        } else {
            i32 count = 0;
            while (string[count]) count += 1;
            return count;
        }
    }

    static void strcat(slice<char> dst_slice, cstr src) {
        char* dst = dst_slice.ptr;
        assert(strlen(dst) + strlen(src) + 1 <= dst_slice.count);

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

    static void memzero(slice<u8> dst) {
        if constexpr (MSVC_COMPILER) {
            size_t size = cast<size_t>(dst.get_size());
            std::memset(dst.ptr, 0, size);
        } else {
            for (u8& byte : dst) byte = 0;
        }
    }

    static void memcpy(void* dst, void* src, size_t size) {
        if constexpr (MSVC_COMPILER) {
            std::memcpy(dst, src, size);
        } else {
            for (size_t i = 0; i < size; ++i) {
                cast<u8*>(dst)[i] = cast<u8*>(src)[i];
            }
        }
    }

    // static void memcpy(slice<u8> dst, slice<u8> src) {
    //     assert_no_overlap(dst, src);
    //     assert(dst.count >= src.count);

    //     if constexpr (MSVC_COMPILER) {
    //         size_t size = cast<size_t>(src.get_size());
    //         std::memcpy(dst.ptr, src.ptr, size);
    //     } else {
    //         for (i64 i = 0; i < src.count; ++i) {
    //             dst(i) = src(i);
    //         }
    //     }
    // }

    static result<i32> find_set_bit_right(u32 value) {
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