#pragma once

// TODO FEAT: move to clang
// TODO FEAT: revive windows xp build

#if !HANDMADE_SLOW
    #define NDEBUG
#endif

#include <cassert>
#include <cmath>
#include <cstdint>

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

static constexpr f64 pi64 = 3.14159265358979323846;
static constexpr f32 pi32 = (f32)pi64;

static constexpr size_t operator ""_KB(u64 value) { return (size_t)(value << 10); }
static constexpr size_t operator ""_MB(u64 value) { return (size_t)(value << 20); }
static constexpr size_t operator ""_GB(u64 value) { return (size_t)(value << 30); }

static inline u32 SafeTruncateToU32(s64 value) {
    assert(value >= 0 && value <= UINT32_MAX);
    return (u32)value;
}

namespace Platform {
    struct ReadEntireFileResult {
        u32 memorySize;
        void* memory;
    };

    static ReadEntireFileResult ReadEntireFile(const char* fileName);
    static void FreeFileMemory(void* memory);
    static bool WriteEntireFile(const char* fileName, const void* memory, u32 memorySize);
}