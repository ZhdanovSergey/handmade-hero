#pragma once

#if !HANDMADE_SLOW
    #define NDEBUG
#endif

#define _USE_MATH_DEFINES

#include <cassert>
#include <cmath>
#include <cstdint>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

static constexpr f64 pi64 = M_PI;
static constexpr f32 pi32 = static_cast<f32>(pi64);

#define KB(value) static_cast<size_t>((value##ull) << 10)
#define MB(value) static_cast<size_t>((value##ull) << 20)
#define GB(value) static_cast<size_t>((value##ull) << 30)

static inline u32 SafeTruncateToU32(s64 value) {
    assert(value >= 0 && value <= UINT32_MAX);
    return static_cast<u32>(value);
}

namespace Platform {
    struct ReadEntireFileResult {
        u32 memorySize;
        std::byte __padding[4];
        void* memory;
    };

    static ReadEntireFileResult ReadEntireFile(const char* fileName);
    static void FreeFileMemory(void* memory);
    static bool WriteEntireFile(const char* fileName, const void* memory, u32 memorySize);
}