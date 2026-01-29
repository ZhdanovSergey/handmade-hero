#pragma once

// TODO: оживить windows xp билд
// TODO: подключить что-нибудь для вывода инфы в консоль вне платформенного слоя
#include <cmath>
#include <cstdint>
#include <cstdio>

using f32 = float;
using f64 = double;

// TODO: использовать знаковые числа по умолчанию
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = size_t;

#if SLOW_MODE
    #define assert(expr) if (!(expr)) *(int*)nullptr = 0
#else
    #define assert(expr)
#endif

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 DOUBLE_PI32 = 2.0f * (f32)PI64;

// TODO: использовать i64 вместо usize
static constexpr usize operator ""_KB(u64 value) { return (usize)(value << 10); }
static constexpr usize operator ""_MB(u64 value) { return (usize)(value << 20); }
static constexpr usize operator ""_GB(u64 value) { return (usize)(value << 30); }

namespace hm {
    template <typename T, usize N>
    static constexpr usize array_count(const T (&)[N]) { return N; }

    template <typename T>
    static inline T min(T a, T b) { return a < b ? a : b; }

    static inline void memcpy(void* dest, const void* src, usize size) {
        for (usize i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((u8*)src)[i];
        }
    }

    static inline void memset(void* dest, u8 value, usize size) {
        for (usize i = 0; i < size; i++) {
            ((u8*)dest)[i] = value;
        }
    }

    static inline usize strlen(const char* src) {
        usize length = 0;
        while (*src++) { length++; }
        return length + 1; // учитываем 0 чтобы результат был такой же как у sizeof
    }

    static inline void strcat(
        const char* src1, usize src1_size,
        const char* src2, usize src2_size,
              char* dest, usize dest_size) {
        assert(src1_size + src2_size <= dest_size);
        usize src1_size_refined = min(src1_size, dest_size);
        usize src2_size_refined = min(src2_size, dest_size - src1_size_refined);
        memcpy(dest, src1, src1_size_refined);
        memcpy(dest + src1_size_refined, src2, src2_size_refined);
        char* last_char = dest + src1_size_refined + src2_size_refined;
        *last_char = 0;
    }
}

namespace Platform {
    struct Read_File_Result {
        usize memory_size;
        void* memory;
    };

    Read_File_Result read_file_sync(const char* filename);
    using Read_File_Sync = decltype(read_file_sync);

    bool write_file_sync(const char* filename, const void* memory, u32 memory_size);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void* memory);
    using Free_File_Memory = decltype(free_file_memory);
}