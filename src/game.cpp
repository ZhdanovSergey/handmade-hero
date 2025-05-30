#include "globals.h"

namespace Game {
	struct ScreenBuffer {
		uint32* memory;
		uint32 width;
		uint32 height;
	};

	struct SoundBuffer {
		int16* samples;
		uint32 samplesPerSecond;
		uint32 samplesCount;
	};

	static void RenderGradient(ScreenBuffer* screenBuffer) {
		uint32* pixel = static_cast<uint32*>(screenBuffer->memory);

		for (uint32 y = 0; y < screenBuffer->height; y++) {
			for (uint32 x = 0; x < screenBuffer->width; x++) {
				uint32 green = y & UINT8_MAX;
				uint32 blue = x & UINT8_MAX;
				*pixel++ = (green << 8) | blue; // padding red green blue
			}
		}
	};

	static void OutputSound(SoundBuffer* soundBuffer) {
		static real32 tSine = 0.0f;
		
		const uint32 frequency = 261;
		const real32 volume = 5000.0f;

		real32 samplesPerWavePeriod = static_cast<real32>(soundBuffer->samplesPerSecond / frequency);
		int16* sampleOut = soundBuffer->samples;

		for (uint32 i = 0; i < soundBuffer->samplesCount; i++) {
			int16 sampleValue = static_cast<int16>(sinf(tSine) * volume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			tSine += 2.0f * PI32 / samplesPerWavePeriod;
		}
	}

	static void UpdateAndRender(ScreenBuffer* screenBuffer, SoundBuffer* soundBuffer) {
		RenderGradient(screenBuffer);
		OutputSound(soundBuffer);
	};
}