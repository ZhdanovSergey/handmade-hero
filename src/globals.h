#pragma once

#define _ALLOW_RTCc_IN_STL

#include <stdint.h>

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

const f32 PI32 = 3.14159265359f;
const f64 PI64 = 3.14159265358979323846;

#define Kilobytes(value) ((value) * 1024ull)
#define Megabytes(value) (Kilobytes(value) * 1024ull)
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

#if HANDMADE_SLOW
#define Assert(expression) if (!(expression)) { * (int*) nullptr = 0; }
#else
#define Assert(expression)
#endif