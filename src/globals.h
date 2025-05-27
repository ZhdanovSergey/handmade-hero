#if !defined(GLOBALS_H)

#define _ALLOW_RTCc_IN_STL

#include <math.h>
#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

const real32 PI32 = 3.14159265359f;
const real64 PI64 = 3.14159265358979323846;

#define GLOBALS_H
#endif