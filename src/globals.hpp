#pragma once

// TODO: подключить что-нибудь для вывода инфы в консоль вне платформенного слоя
#include <cstdint>
#include <cstdio>

using f32 = float;
using f64 = double;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

#if SLOW_MODE
    #define assert(expr) if (!(expr)) *(int*)nullptr = 0
#else
    #define assert(expr)
#endif

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 PI32 = (f32)PI64;
static constexpr f32 DOUBLE_PI32 = 2.0f * PI32;

static constexpr i64 operator ""_KB(u64 value) { return (i64)(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return (i64)(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return (i64)(value << 30); }

namespace hm {
    template <typename T>
    using predicate = bool (*)(const T&);
    
    template <typename T, i32 N>
    static constexpr i32 array_size(const T (&)[N]) { return N; }

    static i32 safe_trunc_i32(i64 value) {
		assert((i32)value == value);
        return (i32)value;
    }
    
    template <typename T>
    struct span {
        T* ptr;
        i64 size;

        span() : ptr{}, size{} {}
        span(T* ptr, i64 size) : ptr{ptr}, size{size} {}

        template <i64 N>
        span(T (&array)[N]) : ptr{array}, size{N} {}

        template <typename U> // позволяет присвоить span<char> в span<const u8>
        span(const span<U>& other) : size{other.size} {
            if constexpr (sizeof(T) == 1) {
                ptr = reinterpret_cast<T*>(other.ptr);
            } else {
                ptr = other.ptr;
            }
        }

        i64 count()   { return size / (i64)sizeof(T); }
        T* begin()    { return ptr; }
        T* end()      { return ptr + count(); }
        T& operator[](i64 index) {
            assert(index >= 0 && index < count());
            return ptr[index];
        }

        i64 find_last_index(predicate<T> predicate) {
            for (i64 index = count() - 1; index >= 0; index--) {
                // TODO: если использовать this[index], то не сходится константность
                if (predicate(ptr[index])) return index;
            }
            return -1;
        }
    };
    template <typename T>
    span(T*, i64) -> span<T>;

    template <typename T>
    static T   min(T a, T b)     { return a * (a <= b) + b * (b < a); }
    static i32 max(i32 a, i32 b) { return a * (a >= b) + b * (b > a); }
    static i32 ceil (f32 x)      { return (i32)x + (x > (i32)x); }
    static i32 floor(f32 x)      { return (i32)x - (x < (i32)x); }
    static i32 round(f32 x)      { return (i32)(x + 0.5f * ((x > 0) - (x < 0))); }

    template <typename T>
    static void memzero(T* dest) {
        for (i64 i = 0; i < sizeof(T); i++) {
            ((u8*)dest)[i] = 0;
        }
    }

    static void memcpy(span<const u8> src, span<u8> dest) {
        assert(src.size <= dest.size);
        for (i64 i = 0; i < min(src.size, dest.size); i++) {
            dest[i] = src[i];
        }
    }

    // TODO: заменить на версию со span
    static void memcpy(void* dest, const void* src, i64 size) {
        for (i64 i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((u8*)src)[i];
        }
    }

    static void strcat(span<const char> src1, span<const char> src2, span<char> dest) {
        assert(src1.size + src2.size <= dest.size);
        src1.size = min(src1.size, dest.size);
        src2.size = min(src2.size, dest.size - src1.size);
        memcpy(src1, span{ dest.ptr, src1.size });
        memcpy(src2, span{ dest.ptr + src1.size, src2.size });
        char* last_char = dest.ptr + src1.size + src2.size;
        *last_char = 0;
    }
}

namespace Platform {
    hm::span<u8> read_file_sync(const char* file_name);
    using Read_File_Sync = decltype(read_file_sync);

    bool write_file_sync(const char* file_name, hm::span<const u8> file);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void*& memory);
    using Free_File_Memory = decltype(free_file_memory);
}