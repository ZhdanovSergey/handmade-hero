#pragma once

// TODO: подключить что-нибудь для вывода дебаг инфы за пределами платформенного слоя (OutputDebugStringA через Game::Memory?)
// TODO: оживить windows xp билд (возможно нужно будет добавить WINAPI для xinput)

#include <cmath>
#include <cstdint>
#include <cstdio>

typedef size_t uptr;
typedef float f32;
typedef double f64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

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

namespace utils {
    template <typename T, uptr N>
    constexpr uptr array_count(const T (&)[N]) { return N; }

    static void memcpy(void* dest, const void* src, uptr size) {
        for (uptr i = 0; i < size; i++) {
            ((u8*)dest)[i] = ((u8*)src)[i];
        }
    }

    static void memset(void* dest, int value, uptr size) {
        for (uptr i = 0; i < size; i++) {
            ((u8*)dest)[i] = (u8)value;
        }
    }

    static void concat_strings(
        const char* src1, uptr src1_size,
        const char* src2, uptr src2_size,
              char* dest, uptr dest_size) {
        uptr src1_size_refined = src1_size < dest_size ? src1_size : dest_size;
        uptr src2_size_refined = src1_size_refined + src2_size < dest_size ? src2_size : dest_size - src1_size_refined;
        memcpy(dest, src1, src1_size_refined);
        memcpy(dest + src1_size_refined, src2, src2_size_refined);
        dest += 0;
    }
}

namespace Platform {
    struct Read_File_Result {
        uptr memory_size;
        void* memory;
    };

    typedef Read_File_Result Read_File_Sync(const char* file_name);
            Read_File_Result read_file_sync(const char* file_name);

    typedef bool Write_File_Sync(const char* file_name, const void* memory, u32 memory_size);
            bool write_file_sync(const char* file_name, const void* memory, u32 memory_size);

    typedef void Free_File_Memory(void* memory);
            void free_file_memory(void* memory);
}