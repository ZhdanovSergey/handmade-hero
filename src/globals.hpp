#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>

// включаем/выключаем assert
#if SLOW_MODE
    #undef NDEBUG
#else
    #define NDEBUG
#endif

using i8  = int8_t;
using u8  = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

template <typename T> struct remove_cv                   { using type = T; };
template <typename T> struct remove_cv<const T>          { using type = T; };
template <typename T> struct remove_cv<volatile T>       { using type = T; };
template <typename T> struct remove_cv<const volatile T> { using type = T; };
template <typename T> using  remove_cv_t = typename remove_cv<T>::type;

template <typename T, typename U> struct is_same_qualified      { static constexpr bool value = false; };
template <typename T>             struct is_same_qualified<T,T> { static constexpr bool value = true; };
template <typename T, typename U> constexpr bool is_same_v = is_same_qualified< remove_cv_t<T>, remove_cv_t<U> >::value;

template <typename T> struct is_number_unqualified           { static constexpr bool value = false; };
template <> struct is_number_unqualified<char>               { static constexpr bool value = true; };
template <> struct is_number_unqualified<signed char>        { static constexpr bool value = true; };
template <> struct is_number_unqualified<unsigned char>      { static constexpr bool value = true; };
template <> struct is_number_unqualified<signed short>       { static constexpr bool value = true; };
template <> struct is_number_unqualified<unsigned short>     { static constexpr bool value = true; };
template <> struct is_number_unqualified<signed int>         { static constexpr bool value = true; };
template <> struct is_number_unqualified<unsigned int>       { static constexpr bool value = true; };
template <> struct is_number_unqualified<signed long>        { static constexpr bool value = true; };
template <> struct is_number_unqualified<unsigned long>      { static constexpr bool value = true; };
template <> struct is_number_unqualified<signed long long>   { static constexpr bool value = true; };
template <> struct is_number_unqualified<unsigned long long> { static constexpr bool value = true; };
template <> struct is_number_unqualified<float>              { static constexpr bool value = true; };
template <> struct is_number_unqualified<double>             { static constexpr bool value = true; };
template <typename T> constexpr bool is_number_v = is_number_unqualified<remove_cv_t<T>>::value;

enum Cast_Flags : u32 {
    DEFAULT         = 0,
    IGNORE_SIGN     = 1 << 0,
    IGNORE_OVERFLOW = 1 << 1,
};

template <typename Out, Cast_Flags Flags = DEFAULT, typename In>
static constexpr Out cast(In value) {
    if constexpr (!(Flags & IGNORE_SIGN) && is_number_v<In> && is_number_v<Out>) {
        assert((value == 0 && (Out)value == 0) ||
               (value >  0 && (Out)value >= 0) ||
               (value <  0 && (Out)value <= 0));
    }
    if constexpr (!(Flags & IGNORE_OVERFLOW) && is_number_v<In> && is_number_v<Out>) {
        if constexpr (is_same_v<In, f32> || is_same_v<In, f64>) {
            assert(value - (In)(Out)value > -1 &&
                   value - (In)(Out)value <  1);
        } else {
            assert((value == (In)(Out)value));
        }
    }
    return (Out)value;
}

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 PI32 = cast<f32>(PI64);
static constexpr f32 DOUBLE_PI32 = 2.0f * PI32;

static constexpr i64 operator ""_KB(u64 value) { return cast<i64>(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return cast<i64>(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return cast<i64>(value << 30); }

template <typename F> struct Deferrer { F f; ~Deferrer() { f(); } };
template <typename F> Deferrer(F) -> Deferrer<F>;
#define CONCAT_INTERNAL(a, b) a##b
#define CONCAT(a, b) CONCAT_INTERNAL(a, b)
#define defer(code) Deferrer CONCAT(defer_, __LINE__){[&](){ code; }}

#define size_of(value) cast<i64>(sizeof(value))

template <typename T, i32 Count_X, i32 Count_Y = 1, i32 Count_Z = 1>
struct static_slice {
    T* base;

    static_slice() = default;
    template <typename U, i32 X>
    static_slice(U (&arr)[X])       : base{reinterpret_cast<U*>(arr)} {
        static_assert(X == Count_X);
    }
    template <typename U, i32 X, i32 Y>
    static_slice(U (&arr)[Y][X])    : base{reinterpret_cast<U*>(arr)} {
        static_assert(X == Count_X && Y == Count_Y);
    }    
    template <typename U, i32 X, i32 Y, i32 Z>
    static_slice(U (&arr)[Z][Y][X]) : base{reinterpret_cast<U*>(arr)} {
        static_assert(X == Count_X && Y == Count_Y && Z == Count_Z);
    }
    template <typename U, i32 X, i32 Y, i32 Z>
    static_slice(static_slice<U, X, Y, Z> other) : base{other.base} {
        static_assert(X == Count_X && Y == Count_Y && Z == Count_Z);
    }

    T* begin() { return base; }
    T* end()   { return base + Count_X * Count_Y * Count_Z; }
    T& operator()(i32 x, i32 y = 0, i32 z = 0) {
        assert(x >= 0 && x < Count_X);
        assert(y >= 0 && y < Count_Y);
        assert(z >= 0 && z < Count_Z);
        return base[ z * Count_Y * Count_X + y * Count_X + x];
    }
    i64 get_size() const { return size_of(T) * Count_X * Count_Y * Count_Z; }
    i32 get_count_x()    { return Count_X; }
    i32 get_count_y()    { return Count_Y; }
    i32 get_count_z()    { return Count_Z; }
};
template <typename T, i32 X>
static_slice(T (&)[X])       -> static_slice<T, X>;
template <typename T, i32 X, i32 Y>
static_slice(T (&)[Y][X])    -> static_slice<T, X, Y>;
template <typename T, i32 X, i32 Y, i32 Z>
static_slice(T (&)[Z][Y][X]) -> static_slice<T, X, Y, Z>;

template <typename T>
struct slice3 {
    T* base;
    i32 count_x;
    i32 count_y;
    i32 count_z;

    slice3() = default;
    template <typename U>
    slice3(slice3<U> other) : base{other.base}, count_x{other.count_x}, count_y{other.count_y}, count_z{other.count_z} {}

    T* begin() { return base; }
    T* end()   { return base + count_x * count_y * count_z; }
    T& operator()(i32 x, i32 y, i32 z) {
        assert(x >= 0 && x < count_x);
        assert(y >= 0 && y < count_y);
        assert(z >= 0 && z < count_z);
        return base[ z * count_y * count_x + y * count_x + x];
    }
    i64 get_size() const { return size_of(T) * count_x * count_y * count_z; }
};
template <typename T>
slice3(T*) -> slice3<T>;

template <typename T>
struct slice2 {
    T* base;
    i32 count_x;
    i32 count_y;

    slice2() = default;
    template <typename U>
    slice2(slice2<U> other) : base{other.base}, count_x{other.count_x}, count_y{other.count_y} {}

    T* begin() { return base; }
    T* end()   { return base + count_x * count_y; }
    T& operator()(i32 x, i32 y) {
        assert(x >= 0 && x < count_x);
        assert(y >= 0 && y < count_y);
        return base[y * count_x + x];
    }
    i64 get_size() const { return size_of(T) * count_x * count_y; }
};
template <typename T>
slice2(T*) -> slice2<T>;

template <typename T>
struct slice1 {
    T* base;
    i64 count;

    slice1() = default;
    template <typename U>
    slice1(slice1<U> other) : slice1{ other.base, other.count } {}
    template <typename U>
    slice1(U* base, i64 count) {
        // финальный конструктор для возможной конвертации в u8
        if constexpr (is_same_v<T,u8>) {
            this->base  = reinterpret_cast<T*>(base);
            this->count = count * size_of(U);
        } else {
            static_assert(is_same_v<T,U>);
            this->base  = base;
            this->count = count;
        }
    }

    T* begin() { return base; }
    T* end()   { return base + count; }
    T& operator()(i64 index) {
        assert(index >= 0 && index < count);
        return base[index];
    }
    i64  get_size() const { return count * size_of(T); }
    void set_size(i64 size) {
        count = size / size_of(T);
        assert(size == count * size_of(T));
    }
};
template <typename T>
slice1(T*) -> slice1<T>;

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
    template <typename Out = i32>
    static constexpr Out round (f32 x) { return cast<Out>(cast<i32>(x + 0.5 * sign(x))); }
    static constexpr i32 ceil  (f32 x) { i32 x_trunc = cast<i32>(x); return x_trunc + (x_trunc < x); }
    static constexpr i32 floor (f32 x) { i32 x_trunc = cast<i32>(x); return x_trunc - (x_trunc > x); }

    template <typename T, i32 N>
    static constexpr i32 array_count(const T (&)[N]) { return N; }
    
    template <typename T>
    static void swap(T& a, T& b) { T temp = a; a = b; b = temp; }

    static void memzero(slice1<u8> slice) {
        for (u8& byte : slice) byte = 0;
    }

    static void memcpy(slice1<const u8> src, slice1<u8> dest) {
        assert(src.count <= dest.count);
        for (i64 i = 0; i < min(src.count, dest.count); ++i) {
            dest(i) = src(i);
        }
    }
}

namespace Platform {
    struct Thread_Context {};

    slice1<u8> read_entire_file(const Thread_Context& thread, const char* file_name);
    using Read_Entire_File = decltype(read_entire_file);

    void write_entire_file(const Thread_Context& thread, const char* file_name, slice1<const u8> file);
    using Write_Entire_File = decltype(write_entire_file);

    void free_file_memory(const Thread_Context& thread, void*& memory);
    using Free_File_Memory = decltype(free_file_memory);
}