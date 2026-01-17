#pragma once

// TODO: оживить windows xp билд
// TODO: подключить что-нибудь для вывода инфы в консоль вне платформенного слоя
#include <cmath>
#include <cstdint>
#include <cstdio>

using uptr = size_t;
using f32  = float;
using f64  = double;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

#if SLOW_MODE
    #define assert(expression) if (!(expression)) *(int*)nullptr = 0
#else
    #define assert(expression)
#endif

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 DOUBLE_PI32 = 2.0f * (f32)PI64;

static constexpr uptr operator ""_KB(u64 value) { return (uptr)(value << 10); }
static constexpr uptr operator ""_MB(u64 value) { return (uptr)(value << 20); }
static constexpr uptr operator ""_GB(u64 value) { return (uptr)(value << 30); }

namespace hm {
    template <typename T, uptr N>
    static constexpr uptr array_count(const T (&)[N]) { return N; }

    template <typename T>
    static inline T min(T a, T b) { return a < b ? a : b; }

    static inline void memcpy(void* dest, const void* src, uptr size) {
        for (uptr i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((u8*)src)[i];
        }
    }

    static inline void memset(void* dest, u8 value, uptr size) {
        for (uptr i = 0; i < size; i++) {
            ((u8*)dest)[i] = value;
        }
    }

    static inline uptr strlen(const char* src) {
        uptr length = 0;
        while (*src++) { length++; }
        return length + 1; // учитываем 0 чтобы результат был такой же как у sizeof
    }

    static inline void strcat(
        const char* src1, uptr src1_size,
        const char* src2, uptr src2_size,
              char* dest, uptr dest_size) {
        assert(src1_size + src2_size <= dest_size);
        uptr src1_size_refined = min(src1_size, dest_size);
        uptr src2_size_refined = min(src2_size, dest_size - src1_size_refined);
        memcpy(dest, src1, src1_size_refined);
        memcpy(dest + src1_size_refined, src2, src2_size_refined);
        char* last_char = dest + src1_size_refined + src2_size_refined;
        *last_char = 0;
    }
}

namespace Platform {
    struct Read_File_Result {
        uptr memory_size;
        void* memory;
    };

    Read_File_Result read_file_sync(const char* filename);
    using Read_File_Sync = decltype(read_file_sync);

    bool write_file_sync(const char* filename, const void* memory, u32 memory_size);
    using Write_File_Sync = decltype(write_file_sync);

    void free_file_memory(void* memory);
    using Free_File_Memory = decltype(free_file_memory);
}