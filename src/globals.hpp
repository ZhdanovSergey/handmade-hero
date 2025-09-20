#pragma once

// TODO FEAT: move to clang
// TODO FEAT: revive windows xp build

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

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

#if INTPTR_MAX == INT64_MAX
    #define _PADDING(value) char CONCAT(_padding, __LINE__)[value];
    #define PADDING_4 _PADDING(4)
#else
    #define _PADDING(value) char CONCAT(_padding, __LINE__)[value % 4];
    #define PADDING_4
#endif

#define PADDING_1 _PADDING(1)
#define PADDING_2 _PADDING(2)
#define PADDING_3 _PADDING(3)
#define PADDING_5 _PADDING(5)
#define PADDING_6 _PADDING(6)
#define PADDING_7 _PADDING(7)

static constexpr f64 pi64 = M_PI;
static constexpr f32 pi32 = static_cast<f32>(pi64);

static constexpr size_t operator ""_KB(u64 value) { return static_cast<size_t>((value << 10) & SIZE_MAX); }
static constexpr size_t operator ""_MB(u64 value) { return static_cast<size_t>((value << 20) & SIZE_MAX); }
static constexpr size_t operator ""_GB(u64 value) { return static_cast<size_t>((value << 30) & SIZE_MAX); }

static inline u32 SafeTruncateToU32(s64 value) {
    assert(value >= 0 && value <= UINT32_MAX);
    return static_cast<u32>(value);
}

namespace Platform {
    struct ReadEntireFileResult {
        u32 memorySize;
        PADDING_4
        void* memory;
    };

    static ReadEntireFileResult ReadEntireFile(const char* fileName);
    static void FreeFileMemory(void* memory);
    static bool WriteEntireFile(const char* fileName, const void* memory, u32 memorySize);
}