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

// TODO: использовать вместо C-style каста
template <typename Out, typename In>
static constexpr Out cast(In value) { return (Out)value; }

// TODO: ограничить типы для trunc? float -> T, i64 -> i32, u64 -> u32
// но вообще даже если это будет аналог cast, использовать все равно имеет смысл
template <typename Out, typename In>
static constexpr Out trunc(In value) { return cast<Out>(value); }

template <typename Out, typename In>
static constexpr Out safe_down_cast(In value) {
    Out casted_value = cast<Out>(value);
    assert(casted_value == value);
    return casted_value;
}

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 PI32 = trunc<f32>(PI64);
static constexpr f32 DOUBLE_PI32 = 2.0f * PI32;

static constexpr i64 operator ""_KB(u64 value) { return cast<i64>(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return cast<i64>(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return cast<i64>(value << 30); }

template <typename F>
struct Deferrer {
    F f;
    ~Deferrer() { f(); }
};
template <typename F>
Deferrer(F) -> Deferrer<F>;
#define defer(code) Deferrer CONCAT(defer_, __LINE__){[&](){ code; }}

#define size_of(value) cast<i64>(sizeof(value))

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
        static_assert(is_same<T,u8>::value || is_same<T,const u8>::value
                   || is_same<T,U >::value || is_same<T,const U >::value);
    }
    template <typename U>
    slice(slice<U> other) : slice{other.base, other.size} {}
    template <typename U, i64 N>
    slice(U (&arr)[N])    : slice{reinterpret_cast<U*>(arr), size_of(arr)} {}
    template <typename U, i64 N, i64 M>
    slice(U (&arr)[N][M]) : slice{reinterpret_cast<U*>(arr), size_of(arr)} {}

    i64 count()   { return size / size_of(T); }
    T* begin()    { return base; }
    T* end()      { return base + count(); }
    T& operator[](i64 index) {
        if constexpr (SLOW_MODE) {
            i64 max_index = count();
            assert(index >= 0 && index < max_index);
        }
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
    T* new_item = cast<T*>(cast<u8*>(arena.base) + arena.used);
    arena.used += size_of(T);
    assert(arena.used <= arena.size);
    return *new_item;
}

template <typename T>
static slice<T> arena_push(Arena<T>& arena, i64 count) {
    slice<T> new_slice = {};
    new_slice.base = cast<T*>(cast<u8*>(arena.base) + arena.used);
    new_slice.size = size_of(T) * count;
    
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
    static T   min(T a,   T b)   { return a < b ? a : b; }
    static i32 max(i32 a, i32 b) { return a > b ? a : b; }
    template <typename T>
    static T   sign  (T x)   { return cast<T>((x > 0) - (x < 0)); }
    static i16 abs   (i16 x) { return sign(x) * x; }
    static i32 round (f32 x) { return trunc<i32>(x + 0.5f * sign(x)); }
    static i32 ceil  (f32 x) { i32 x_trunc = trunc<i32>(x); return x_trunc + (x > x_trunc); }
    static i32 floor (f32 x) { i32 x_trunc = trunc<i32>(x); return x_trunc - (x < x_trunc); }

    template <typename T, i32 N>
    static constexpr i32 array_size(const T (&)[N]) { return N; }
    
    template <typename T>
    static void swap(T& a, T& b) { T temp = a; a = b; b = temp; }

    static void memzero(slice<u8> slice) {
        for (auto& byte : slice) byte = 0;
    }

    static void memcpy(slice<const u8> src, slice<u8> dest) {
        assert(src.size <= dest.size);
        for (i64 i = 0; i < min(src.size, dest.size); ++i) {
            dest[i] = src[i];
        }
    }

    // TODO: придумать что делать с возможным наличием/отсутствием null в конце src1
    static void strcat(slice<const char> src1, slice<const char> src2, slice<char> dest) {
        assert(src1.size + src2.size <= dest.size);
        src1.size = min(src1.size, dest.size);
        src2.size = min(src2.size, dest.size - src1.size);
        memcpy(src1, slice{ dest.base, src1.size });
        memcpy(src2, slice{ dest.base + src1.size, src2.size });
        char* last_char = dest.base + src1.size + src2.size - 1;
        *last_char = 0;
    }
}

namespace Platform {
    slice<u8> read_file_sync(const char* file_name);
    using Read_File_Sync = decltype(read_file_sync);

    void write_file_sync(const char* file_name, slice<const u8> file);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void*& memory);
    using Free_File_Memory = decltype(free_file_memory);
}