#pragma once

// TODO: подключить что-нибудь для вывода инфы в консоль вне платформенного слоя
#include <cmath>
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
static constexpr f32 DOUBLE_PI32 = 2.0f * (f32)PI64;

static constexpr i64 operator ""_KB(u64 value) { return (i64)(value << 10); }
static constexpr i64 operator ""_MB(u64 value) { return (i64)(value << 20); }
static constexpr i64 operator ""_GB(u64 value) { return (i64)(value << 30); }

namespace hm {
    template <typename T, i32 N>
    static constexpr i32 array_size(const T (&)[N]) { return N; }

    static inline f32 ceilf(f32 x) {
        f32 x_trunc = (f32)(i32)x;
        return x > 0 && x > x_trunc ? x_trunc + 1.0f : x_trunc;
    }

    static inline i32 min(i32 a, i32 b) { return a < b ? a : b; }

    static inline void memcpy(void* dest, const void* src, i32 size) {
        for (i32 i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((u8*)src)[i];
        }
    }

    static inline void memset(void* dest, u8 value, i32 size) {
        for (i32 i = 0; i < size; i++) {
            ((u8*)dest)[i] = value;
        }
    }

    static inline i32 strlen(const char* src) {
        i32 length = 0;
        while (*src++) { length++; }
        return length + 1; // учитываем 0 чтобы результат был такой же как у sizeof
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