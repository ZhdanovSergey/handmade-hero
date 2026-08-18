#pragma once

#if SLOW_MODE
    #pragma inline_depth(0) // выключаем инлайнинг кроме __forceinline
    #undef NDEBUG           // включаем assert
#else
    #define NDEBUG
#endif

#include <cassert>
#include <cmath>
#include <cstdint>

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
using cstr = const char *;

template <typename T> struct remove_ref      { using type = T; };
template <typename T> struct remove_ref<T&>  { using type = T; };
template <typename T> struct remove_ref<T&&> { using type = T; };
template <typename T> using remove_ref_t = typename remove_ref<T>::type;

template <typename T> struct remove_cv                   { using type = T; };
template <typename T> struct remove_cv<const T>          { using type = T; };
template <typename T> struct remove_cv<volatile T>       { using type = T; };
template <typename T> struct remove_cv<const volatile T> { using type = T; };
template <typename T> using remove_cv_t = typename remove_cv<T>::type;

template <typename T, typename U> struct is_same_exact      { static constexpr bool value = false; };
template <typename T>             struct is_same_exact<T,T> { static constexpr bool value = true; };
template <typename T, typename U> constexpr bool is_same_v = is_same_exact< remove_cv_t<T>, remove_cv_t<U> >::value;

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

template <bool C, typename T, typename F> struct conditional           { using type = F; };
template <typename T, typename F>         struct conditional<true,T,F> { using type = T; };
template <bool C, typename T, typename F> using conditional_t = typename conditional<C,T,F>::type;

template <typename Out, typename In>
__forceinline
static constexpr Out cast(In&& value) {
    using In_Not_Ref = remove_ref_t<In>;
    if constexpr (is_number_v<In_Not_Ref> && is_number_v<Out>) {
        if constexpr (is_same_v<In_Not_Ref, f32> || is_same_v<In_Not_Ref, f64>) {
            assert(value -  (In_Not_Ref)(Out)value > -1);
            assert(value -  (In_Not_Ref)(Out)value <  1);
        } else {
            assert(value == (In_Not_Ref)(Out)value);
        }
    }
    return (Out)(In&&)value;
}

static constexpr f32 PI = 3.1415927f;
static constexpr f32 TWO_PI = 2.0f * PI;
static constexpr f32 SQRT_2 = 1.41421356f;

static constexpr i64 operator ""_KB(u64 value) { return cast<i64>(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return cast<i64>(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return cast<i64>(value << 30); }

template <typename F> struct Deferrer { F f; ~Deferrer() { f(); } };
template <typename F> Deferrer(F) -> Deferrer<F>;
#define CONCAT_INTERNAL(a, b) a##b
#define CONCAT(a, b) CONCAT_INTERNAL(a, b)
#define defer(code) Deferrer CONCAT(defer_, __LINE__){[&](){ code; }}

#define size_of(value) cast<i64>(sizeof(value))

template <typename T>
struct vec2 {
    T x, y;

    template <typename U>
    explicit operator vec2<U>() {
        return { cast<U>(x), cast<U>(y) };
    }
    explicit operator bool() {
        static_assert(is_same_v<T, bool>);
        return x && y;
    }

    __forceinline
    const T& operator()(i32 index) const {
        assert(index >= 0 && index < 2);
        return cast<T*>(this)[index];
    }

    __forceinline
    T& operator()(i32 index) {
        const T& const_result = cast<const vec2<T>&>(*this)(index);
        return cast<T&>(const_result);
    }

    friend vec2<bool> operator==(vec2<T> a, vec2<T> b) { return { a.x == b.x, a.y == b.y }; }

    friend vec2<T> operator+ (vec2<T> a, vec2<T> b) { return { a.x + b.x, a.y + b.y }; }
    friend vec2<T> operator- (vec2<T> a, vec2<T> b) { return { a.x - b.x, a.y - b.y }; }
    friend vec2<T> operator* (vec2<T> a, T num)     { return { a.x * num, a.y * num }; }
    friend vec2<T> operator* (T num, vec2<T> a)     { return { a.x * num, a.y * num }; }
    friend vec2<T> operator/ (vec2<T> a, T num)     { return { a.x / num, a.y / num }; }
    friend vec2<T> operator- (vec2<T> a)            { return { - a.x, - a.y }; }

    friend void operator+=(vec2<T>& a, vec2<T> b) { a = a + b; }
    friend void operator-=(vec2<T>& a, vec2<T> b) { a = a - b; }
    friend void operator*=(vec2<T>& a, T num)     { a = a * num; }
    friend void operator/=(vec2<T>& a, T num)     { a = a / num; }
};

template <typename T, i32 N>
struct Array {
    T ptr[N];

    i32 get_count() { return N; }

    T* begin() { return ptr; }
    T* end()   { return ptr + N; }
    __forceinline
    const T& operator()(i32 index) const {
        assert(index >= 0 && index < N);
        return ptr[index];
    }
    __forceinline
    T& operator()(i32 index) { return cast<T&>(cast<const Array&>(*this)(index)); }
};

template <typename T, i32 Count_X, i32 Count_Y = 1, i32 Count_Z = 1>
struct static_slice {
    T* ptr;

    i64 get_size()    { return size_of(T) * Count_X * Count_Y * Count_Z; }
    i32 get_count_x() { return Count_X; }
    i32 get_count_y() { return Count_Y; }
    i32 get_count_z() { return Count_Z; }

    T* begin() { return ptr; }
    T* end()   { return ptr + Count_X * Count_Y * Count_Z; }
    __forceinline
    T& operator()(i32 x, i32 y = 0, i32 z = 0) {
        assert(x >= 0 && x < Count_X);
        assert(y >= 0 && y < Count_Y);
        assert(z >= 0 && z < Count_Z);
        return ptr[ z * Count_Y * Count_X + y * Count_X + x];
    }
};

template <typename T>
struct slice3 {
    T* ptr;
    i32 count_x;
    i32 count_y;
    i32 count_z;

    i64 get_size() { return size_of(T) * count_x * count_y * count_z; }

    T* begin() { return ptr; }
    T* end()   { return ptr + count_x * count_y * count_z; }
    __forceinline
    T& operator()(i32 x, i32 y, i32 z) {
        assert(x >= 0 && x < count_x);
        assert(y >= 0 && y < count_y);
        assert(z >= 0 && z < count_z);
        return ptr[ z * count_y * count_x + y * count_x + x];
    }
};

template <typename T>
struct slice2 {
    T* ptr;
    vec2<i32> count;

    i64 get_size() { return size_of(T) * count.x * count.y; }

    T* begin() { return ptr; }
    T* end()   { return ptr + count.x * count.y; }
    __forceinline
    T& operator()(i32 x, i32 y) {
        assert(x >= 0 && x < count.x);
        assert(y >= 0 && y < count.y);
        return ptr[y * count.x + x];
    }
};

template <typename T>
struct slice {
    T* ptr;
    i64 count;

    slice() = default;
    template <typename T, i64 X>
    slice(T (&arr)[X]) : slice{arr, X } {}
    template <typename U>
    slice(slice<U>& other) : slice{other.ptr, other.count } {}
    template <typename U>
    slice(U* ptr, i64 count) {
        // финальный конструктор для возможной конвертации в u8
        if constexpr (is_same_v<T,u8>) {
            this->ptr   = cast<u8*>(ptr);
            this->count = count * size_of(U);
        } else {
            static_assert(is_same_v<T,U>);
            this->ptr   = ptr;
            this->count = count;
        }
    }

    i64  get_size() { return count * size_of(T); }
    void set_size(i64 size) {
        count = size / size_of(T);
        assert(size == count * size_of(T));
    }

    T* begin() { return ptr; }
    T* end()   { return ptr + count; }
    __forceinline
    T& operator()(i64 index) {
        assert(index >= 0 && index < count);
        return ptr[index];
    }
};

struct Arena {
    u8* ptr;
    i64 size;
    i64 used;

    void clear() { used = 0; }
    
    template <typename T>
    T* push(i64 new_size) {
        T* new_ptr = cast<T*>(ptr + used);
        used += new_size;
        assert(new_size % size_of(T) == 0);
        assert(used <= size);
        return new_ptr;
    }
};

template <typename T>
struct result {
    bool ok;
    T value;
};

template <typename T>
static void swap(T& a, T& b) { T temp = a; a = b; b = temp; }