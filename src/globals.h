#pragma once

#if !HANDMADE_SLOW
    #define NDEBUG
#endif

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

static const f64 pi64 = 3.141592653589793;
static const f32 pi32 = static_cast<f32>(pi64);

static constexpr u64 operator ""_KB(u64 value) { return value << 10; }
static constexpr u64 operator ""_MB(u64 value) { return value << 20; }
static constexpr u64 operator ""_GB(u64 value) { return value << 30; }

static inline u32 SafeTruncateToU32(s64 value) {
    assert(value >= 0 && value <= UINT32_MAX);
    return static_cast<u32>(value);
}

// TODO: consider use of std::string and std::byte everywhere
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