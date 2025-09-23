#include "game.hpp"

namespace Game {
	static void UpdateAndRender(Input* input, Memory* memory, ScreenBuffer* screenBuffer, SoundBuffer* soundBuffer) {
		assert(sizeof(GameState) <= memory->permanentStorageSize);

		GameState* gameState = static_cast<GameState*>(memory->permanentStorage);

		if (!memory->isInitialized)
			memory->isInitialized = true;

		auto readFileResult = Platform::ReadEntireFile(__FILE__);

		if (readFileResult.memory) {
			Platform::WriteEntireFile("test.out", readFileResult.memory, readFileResult.memorySize);
			Platform::FreeFileMemory(readFileResult.memory);
		}

		if (input->moveUp.isEndedPressed)
			gameState->greenOffset--;

		if (input->moveDown.isEndedPressed)
			gameState->greenOffset++;

		if (input->moveRight.isEndedPressed)
			gameState->blueOffset++;

		if (input->moveLeft.isEndedPressed)
			gameState->blueOffset--;

		RenderGradient(gameState, screenBuffer);
		OutputSound(gameState, soundBuffer);
	};

	static void RenderGradient(GameState* gameState, ScreenBuffer* screenBuffer) {
		for (u32 y = 0; y < screenBuffer->height; y++) {
			for (u32 x = 0; x < screenBuffer->width; x++) {
				u32 green = (y + gameState->greenOffset) & UINT8_MAX;
				u32 blue = (x + gameState->blueOffset) & UINT8_MAX;
				*(screenBuffer->memory)++ = (green << 8) | blue; // padding red green blue
			}
		}
	};

	static void OutputSound(GameState* gameState, SoundBuffer* soundBuffer) {
		u32 frequency = 261;
		f32 volume = 5000.0f;

		f32 samplesPerWavePeriod = static_cast<f32>(soundBuffer->samplesPerSecond / frequency);
		s16* sampleOut = soundBuffer->samples;

		for (u32 i = 0; i < soundBuffer->samplesToWrite; i++) {
			s16 sampleValue = static_cast<s16>(std::sin(gameState->tSine) * volume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			gameState->tSine += 2.0f * pi32 / samplesPerWavePeriod;
		}
	}
}