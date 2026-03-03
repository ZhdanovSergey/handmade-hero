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

#define CONST_METHOD_PAIR(signarure, ...) signarure __VA_ARGS__ \
                                           const signarure const __VA_ARGS__

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 PI32 = (f32)PI64;
static constexpr f32 DOUBLE_PI32 = 2.0f * PI32;

static constexpr i64 operator ""_KB(u64 value) { return (i64)(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return (i64)(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return (i64)(value << 30); }

namespace hm {
    template <typename T, i32 N>
    struct Array {
        T data[N];

        i32 size() const { return N; }
        CONST_METHOD_PAIR(T* begin(), { return data; })
        CONST_METHOD_PAIR(T* end(),   { return data + N; })
        CONST_METHOD_PAIR(T& operator[](i32 index), {
            assert(index >= 0 && index < N);
            return data[index];
        })
    };

    template <typename T>
    struct Span {
        T* ptr;
        i32 size;

        CONST_METHOD_PAIR(T* begin(), { return ptr; })
        CONST_METHOD_PAIR(T* end(),   { return ptr + size; })
        CONST_METHOD_PAIR(T& operator[](i32 index), {
            assert(index >= 0 && index < size);
            return ptr[index];
        })
    };

    static i32 min(i32 a, i32 b) { return a < b ? a : b; }
    static i32 max(i32 a, i32 b) { return a > b ? a : b; }
    static i32 ceil (f32 x) { return (i32)x + (x > (i32)x); }
    static i32 floor(f32 x) { return (i32)x - (x < (i32)x); }
    static i32 round(f32 x) { return (i32)(x + 0.5f * ((x > 0) - (x < 0))); }

    static void memcpy(void* dest, const void* src, i64 size) {
        for (i64 i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((u8*)src)[i];
        }
    }

    static void memset(void* dest, u8 value, i64 size) {
        for (i64 i = 0; i < size; i++) {
            ((u8*)dest)[i] = value;
        }
    }

    static i32 strlen(const char* src) {
        i32 length = 0;
        while (*src++) { length++; }
        return length + 1; // учитываем 0
    }

    static void strcat(
        const char* src1, i32 src1_size,
        const char* src2, i32 src2_size,
              char* dest, i32 dest_size) {
        assert(src1_size + src2_size <= dest_size);
        i32 src1_size_refined = min(src1_size, dest_size);
        i32 src2_size_refined = min(src2_size, dest_size - src1_size_refined);
        memcpy(dest, src1, src1_size_refined);
        memcpy(dest + src1_size_refined, src2, src2_size_refined);
        char* last_char = dest + src1_size_refined + src2_size_refined;
        *last_char = 0;
    }
}

namespace Platform {
    struct Read_File_Result {
        i32 memory_size;
        void* memory;
    };

    Read_File_Result read_file_sync(const char* filename);
    using Read_File_Sync = decltype(read_file_sync);

    bool write_file_sync(const char* filename, const void* memory, i32 memory_size);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void*& memory);
    using Free_File_Memory = decltype(free_file_memory);
}