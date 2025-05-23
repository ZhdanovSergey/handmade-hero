#include "int.h"

struct ScreenBuffer {
    void* memory;
    uint32 width;
    uint32 height;
};

static void GameUpdateAndRender(ScreenBuffer* screenBuffer) {
	uint32* pixel = static_cast<uint32*>(screenBuffer->memory);

	for (uint32 y = 0; y < screenBuffer->height; y++) {
		for (uint32 x = 0; x < screenBuffer->width; x++) {
			uint32 green = y & UINT8_MAX;
			uint32 blue = x & UINT8_MAX;
			*pixel++ = (green << 8) | blue; // padding red green blue
		}
	}
};