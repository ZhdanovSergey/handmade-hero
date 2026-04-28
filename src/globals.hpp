#pragma once

#include <cstdint>
#include <cstdio>

#if SLOW_MODE
    #define assert(expr) if (!(expr)) *(int*)nullptr = 0
#else
    #define assert(expr)
#endif

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

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 PI32 = (f32)PI64;
static constexpr f32 DOUBLE_PI32 = 2.0f * PI32;

static constexpr i64 operator ""_KB(u64 value) { return (i64)(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return (i64)(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return (i64)(value << 30); }

namespace hm {
    template <typename T, typename U>
    struct is_same       { static constexpr bool value = false; };
    template <typename T>
    struct is_same<T, T> { static constexpr bool value = true; };
    
    template <typename T>
    using predicate = bool (*)(T);
    
    // TODO: добавить функционал для многомерных блоков, как в mdspan
    template <typename T>
    struct span {
        T* ptr;
        i64 size;

        span() : ptr{}, size{} {}
        template <typename U>
        span(U* ptr, i64 size) : ptr{reinterpret_cast<T*>(ptr)}, size{size} {
            assert(size % sizeof(T) == 0);
            static_assert(is_same<T,u8>::value || is_same<T,const u8>::value
                       || is_same<T,U >::value || is_same<T,const U >::value);
        }
        template <typename U>
        span(span<U> other) : span{other.ptr, other.size} {}
        template <typename U, i64 N>
        span(U (&arr)[N])    : span{reinterpret_cast<U*>(arr), sizeof(arr)} {}
        template <typename U, i64 N, i64 M>
        span(U (&arr)[N][M]) : span{reinterpret_cast<U*>(arr), sizeof(arr)} {}

        i64 count()   { return size / (i64)sizeof(T); }
        T* begin()    { return ptr; }
        T* end()      { return ptr + count(); }
        T& operator[](i64 index) {
            assert(index >= 0 && index < count());
            return ptr[index];
        }
    };
    template <typename T>
    span(T*, i64) -> span<T>;
    
    template <typename T>
    static i64 find_last_index(span<T> span, predicate<T> predicate) {
        for (i64 index = span.count() - 1; index >= 0; index--) {
            if (predicate(span[index])) return index;
        }
        return -1;
    };

    template <typename T>
    static T   min(T a, T b)     { return a < b ? a : b; }
    static i32 max(i32 a, i32 b) { return a > b ? a : b; }
    static f32 sign(f32 x)       { return (f32)((x > 0) - (x < 0)); }
    static i32 round(f32 x)      { return (i32)(x + 0.5f * sign(x)); }
    static i32 ceil (f32 x)      { return (i32)x + (x > (i32)x); }
    static i32 floor(f32 x)      { return (i32)x - (x < (i32)x); }

    template <typename T, i32 N>
    static constexpr i32 array_size(const T (&)[N]) { return N; }
    
    template <typename T>
    static void swap(T& a, T& b) { T temp = a; a = b; b = temp; }

    template <typename T>
    static void memzero(T* dest) {
        for (i64 i = 0; i < sizeof(T); i++) {
            ((u8*)dest)[i] = 0;
        }
    }

    static i32 trunc_i32(i64 value) {
		assert((i32)value == value);
        return (i32)value;
    }

    static void memcpy(span<const u8> src, span<u8> dest) {
        assert(src.size <= dest.size);
        for (i64 i = 0; i < min(src.size, dest.size); i++) {
            dest[i] = src[i];
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

template <typename T>
using span = hm::span<T>;

namespace Platform {
    span<u8> read_file_sync(const char* file_name);
    using Read_File_Sync = decltype(read_file_sync);

    bool write_file_sync(const char* file_name, span<const u8> file);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void*& memory);
    using Free_File_Memory = decltype(free_file_memory);
}