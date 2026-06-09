#pragma once

#include <cstdint>
#include <cstdio>

#if SLOW_MODE
    #define assert(expr) if (!(expr)) *(int*)nullptr = 0;
#else
    #define assert(expr)
#endif

#define CONCAT_INTERNAL(a, b) a##b
#define CONCAT(a, b) CONCAT_INTERNAL(a, b)

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

static const f64 PI64 = 3.14159265358979323846;
static const f32 PI32 = (f32)PI64;
static const f32 DOUBLE_PI32 = 2.0f * PI32;

static constexpr i64 operator ""_KB(u64 value) { return (i64)(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return (i64)(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return (i64)(value << 30); }

template <typename F>
struct Deferrer {
    F f;
    ~Deferrer() { f(); }
};
#define defer(code) Deferrer CONCAT(_defer_, __LINE__){[&](){ code; }}

template <typename T, typename U>
struct is_same       { static constexpr bool value = false; };
template <typename T>
struct is_same<T, T> { static constexpr bool value = true; };

template <typename T>
using predicate = bool (*)(T);

// TODO: добавить slice для многомерных блоков
template <typename T>
struct slice {
    T* base;
    i64 size;

    slice() : base{}, size{} {}
    template <typename U>
    slice(U* base, i64 size) : base{reinterpret_cast<T*>(base)}, size{size} {
        assert(size % sizeof(T) == 0);
        static_assert(is_same<T,u8>::value || is_same<T,const u8>::value
                    || is_same<T,U >::value || is_same<T,const U >::value);
    }
    template <typename U>
    slice(slice<U> other) : slice{other.base, other.size} {}
    template <typename U, i64 N>
    slice(U (&arr)[N])    : slice{reinterpret_cast<U*>(arr), sizeof(arr)} {}
    template <typename U, i64 N, i64 M>
    slice(U (&arr)[N][M]) : slice{reinterpret_cast<U*>(arr), sizeof(arr)} {}

    i64 count()   { return size / (i64)sizeof(T); }
    T* begin()    { return base; }
    T* end()      { return base + count(); }
    T& operator[](i64 index) {
        assert(index >= 0 && index < count());
        return base[index];
    }
};
template <typename T>
slice(T*, i64) -> slice<T>;

template <typename T>
struct Arena {
    T* base;
    i64 size;
    i64 used;
};

template <typename T>
static T& arena_push(Arena<T>& arena) {
    T* new_item = (T*)((u8*)arena.base + arena.used);    
    arena.used += sizeof(T);
    assert(arena.used <= arena.size);
    return *new_item;
}

template <typename T>
static slice<T> arena_push(Arena<T>& arena, i64 count) {
    slice<T> new_slice = {};
    new_slice.base = (T*)((u8*)arena.base + arena.used);
    new_slice.size = (i64)sizeof(T) * count;
    
    arena.used += new_slice.size;
    assert(arena.used <= arena.size);
    return new_slice;
}

// TODO: вынести hm и Platform в отдельные файлы?
namespace hm {
    template <typename T>
    static i64 find_last_index(slice<T> slice, predicate<T> predicate) {
        for (i64 index = slice.count() - 1; index >= 0; index--) {
            if (predicate(slice[index])) return index;
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
        for (i64 i = 0; i < sizeof(T); ++i) {
            ((u8*)dest)[i] = 0;
        }
    }

    static i32 trunc_i32(i64 value) {
		assert((i32)value == value);
        return (i32)value;
    }

    static void memcpy(slice<const u8> src, slice<u8> dest) {
        assert(src.size <= dest.size);
        for (i64 i = 0; i < min(src.size, dest.size); ++i) {
            dest[i] = src[i];
        }
    }

    static void strcat(slice<const char> src1, slice<const char> src2, slice<char> dest) {
        assert(src1.size + src2.size - 1 <= dest.size);
        src1.size = min(src1.size, dest.size);
        src2.size = min(src2.size, dest.size - src1.size);
        memcpy(src1, slice{ dest.base, src1.size });
        memcpy(src2, slice{ dest.base + src1.size, src2.size });
        char* last_char = dest.base + src1.size + src2.size;
        *last_char = 0;
    }
}

namespace Platform {
    slice<u8> read_file_sync(const char* file_name);
    using Read_File_Sync = decltype(read_file_sync);

    bool write_file_sync(const char* file_name, slice<const u8> file);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void*& memory);
    using Free_File_Memory = decltype(free_file_memory);
}