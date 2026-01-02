#pragma once

// TODO: подключить что-нибудь для вывода дебаг инфы за пределами платформенного слоя
// TODO: оживить windows xp билд (возможно нужно будет добавить WINAPI для xinput)

#if !SLOW_MODE
    #define NDEBUG // выключаем assert
#endif

#include <cassert>
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

static constexpr f64 PI64 = 3.14159265358979323846;
static constexpr f32 PI32 = (f32)PI64;

static constexpr uptr operator ""_KB(u64 value) { return (uptr)(value << 10); }
static constexpr uptr operator ""_MB(u64 value) { return (uptr)(value << 20); }
static constexpr uptr operator ""_GB(u64 value) { return (uptr)(value << 30); }

template <typename T, uptr N>
constexpr uptr array_count(const T (&)[N]) { return N; }

static inline u32 safe_truncate_to_u32(s64 value) {
    assert(value >= 0 && value <= UINT32_MAX);
    return (u32)value;
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