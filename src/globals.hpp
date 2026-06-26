#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

// включаем/выключаем assert
#if SLOW_MODE
    #undef NDEBUG
#else
    #define NDEBUG
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

template <typename T, typename U>
struct is_same       { static constexpr bool value = false; };
template <typename T>
struct is_same<T, T> { static constexpr bool value = true; };

#define assert_cast_preserves_sign(Type_Out, VALUE)      \
    if constexpr (SLOW_MODE) {                           \
        auto orig_value = (VALUE);                       \
        auto casted_value = (Type_Out)(orig_value);      \
        assert((orig_value == 0)                         \
            || (orig_value > 0 && casted_value >= 0)     \
            || (orig_value < 0 && casted_value <= 0));   \
    }

#define assert_cast_preserves_value(Type_Out, VALUE)                                      \
    if constexpr (SLOW_MODE) {                                                            \
        using Type_In = decltype(VALUE);                                                  \
        auto orig_value = (VALUE);                                                        \
        auto casted_in_and_out = (Type_In)(Type_Out)(orig_value);                         \
                                                                                          \
        if constexpr (is_same<Type_In, f32>::value || is_same<Type_In, f64>::value) {     \
            assert((orig_value - casted_in_and_out) > - 1);                               \
            assert((orig_value - casted_in_and_out) <   1);                               \
        } else {                                                                          \
            assert((orig_value == casted_in_and_out));                                    \
        }                                                                                 \
    }

template <typename Out, typename In>
static constexpr Out cast(In value) {
    assert_cast_preserves_sign(Out, value);
    assert_cast_preserves_value(Out, value);
    return (Out)value;
}

template <typename Out, typename In>
static constexpr Out cast_ignore_sign(In value) {
    assert_cast_preserves_value(Out, value);
    return (Out)value;
}

template <typename Out, typename In>
static constexpr Out cast_ignore_overflow(In value) {
    assert_cast_preserves_sign(Out, value);
    return (Out)value;
}

#define CONCAT_INTERNAL(a, b) a##b
#define CONCAT(a, b) CONCAT_INTERNAL(a, b)
#define size_of(value) cast<i64>(sizeof(value))

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 PI32 = cast<f32>(PI64);
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

template <typename T>
struct slice2;

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
    template <typename U>
    slice(slice2<U> other) : slice{other.base, other.get_size()} {};

    T* begin() { return base; }
    T* end()   { return base + get_count(); }
    T& operator[](i64 index) {
        assert(index >= 0 && index < get_count());
        return base[index];
    }
    void set_count(i64 count) { size = count * size_of(T); }
    i64 get_count() {
        assert(size % size_of(T) == 0);
        return size / size_of(T);
    }
};
template <typename T>
slice(T*, i64) -> slice<T>;

template <typename T>
struct slice2 {
    T* base;
    i32 width;
    i32 height;

    slice2() : base{}, width{}, height{} {}
    template <typename U>
    slice2(slice2<U> other) : slice2{other.base, other.width, other.height} {}
    template <typename U, i32 N, i32 M>
    slice2(U (&arr)[N][M]) : slice2{reinterpret_cast<U*>(arr), M, N} {}

    T* begin()     { return base; }
    T* end()       { return base + width * height; }
    i64 get_size() { return size_of(T) * width * height; }
    T& get(i32 x, i32 y) {
        assert(x >= 0 && x < width);
        assert(y >= 0 && y < height);
        return base[y * width + x];
    }
};
template <typename T>
slice2(T*, i32, i32) -> slice2<T>;

struct Arena {
    u8* base;
    i64 size;
    i64 used;

    void clear() { used = 0; }
    
    template <typename T>
    T* push(i64 new_size) {
        auto& arena = *this;
        T* new_ptr = cast<T*>(arena.base + arena.used);
        arena.used += new_size;
        assert(new_size % size_of(T) == 0);
        assert(arena.used <= arena.size);
        return new_ptr;
    }
};

// LATER: вынести hm и Platform в отдельные файлы?
namespace hm {
    template <typename T>
    static constexpr T   min(T a,   T b)   { return a < b ? a : b; }
    static constexpr i32 max(i32 a, i32 b) { return a > b ? a : b; }
    template <typename T>
    static constexpr T   sign  (T x)   { return cast<T>((x > 0) - (x < 0)); }
    static constexpr i32 abs   (i32 x) { return sign(x) * x; }
    static constexpr i32 round (f32 x) { return cast<i32>(x + 0.5f * sign(x)); }
    static constexpr i32 ceil  (f32 x) { i32 x_trunc = cast<i32>(x); return x_trunc + (x > x_trunc); }
    static constexpr i32 floor (f32 x) { i32 x_trunc = cast<i32>(x); return x_trunc - (x < x_trunc); }

    template <typename T, i32 N>
    static constexpr i32 array_count(const T (&)[N]) { return N; }
    
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
}

namespace Platform {
    slice<u8> read_file_sync(const char* file_name);
    using Read_File_Sync = decltype(read_file_sync);

    void write_file_sync(const char* file_name, slice<const u8> file);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void*& memory);
    using Free_File_Memory = decltype(free_file_memory);
}