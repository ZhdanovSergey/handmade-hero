#pragma once

#include "globals.h"

namespace Game {
	struct ScreenBuffer {
		u32 width;
		u32 height;
		u32* memory;
	};

	struct SoundBuffer {
		u32 samplesPerSecond;
		u32 samplesToWrite;
		s16* samples;
	};

	struct Memory {
		bool isInitialized;
		std::byte __padding[7];
		size_t permanentStorageSize;
		size_t transientStorageSize;
		void* permanentStorage;
		void* transientStorage;
	};

	struct GameState {
		f32 tSine;
	};
}