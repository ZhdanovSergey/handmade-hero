#include "globals.h"

// TODO: delete when sinf will go away
#include <math.h>

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
		u8 __padding[7];
		u64 permanentStorageSize;
		u64 transientStorageSize;
		void* permanentStorage;
		void* transientStorage;
	};

	struct GameState {
		f32 tSine;
	};

	static void RenderGradient(ScreenBuffer* screenBuffer) {
		u32* pixel = static_cast<u32*>(screenBuffer->memory);

		for (u32 y = 0; y < screenBuffer->height; y++) {
			for (u32 x = 0; x < screenBuffer->width; x++) {
				u32 green = y & UINT8_MAX;
				u32 blue = x & UINT8_MAX;
				*pixel++ = (green << 8) | blue; // padding red green blue
			}
		}
	};

	static void OutputSound(GameState* gameState, SoundBuffer* soundBuffer) {		
		const u32 frequency = 261;
		const f32 volume = 5000.0f;

		f32 samplesPerWavePeriod = static_cast<f32>(soundBuffer->samplesPerSecond / frequency);
		s16* sampleOut = soundBuffer->samples;

		for (u32 i = 0; i < soundBuffer->samplesToWrite; i++) {
			s16 sampleValue = static_cast<s16>(sinf(gameState->tSine) * volume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			gameState->tSine += 2.0f * PI32 / samplesPerWavePeriod;
		}
	}

	// TODO: use unified user input even without controller support
	static void UpdateAndRender(Memory* memory, ScreenBuffer* screenBuffer, SoundBuffer* soundBuffer) {
		Assert(sizeof(GameState) <= memory->permanentStorageSize);

		GameState* gameState = static_cast<GameState*>(memory->permanentStorage);

		if (!memory->isInitialized) {
			memory->isInitialized = true;
		}

		RenderGradient(screenBuffer);
		OutputSound(gameState, soundBuffer);
	};
}