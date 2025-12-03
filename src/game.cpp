#include "game.hpp"

namespace Game {
	static void UpdateAndRender(Memory& memory, const Input& input, ScreenBuffer& screenBuffer) {
		assert(sizeof(GameState) <= memory.permanentStorageSize);
		GameState* gameState = (GameState*)memory.permanentStorage;

		if (!memory.isInitialized)
			memory.isInitialized = true;

		auto readFileResult = Platform::ReadEntireFile(__FILE__);

		if (readFileResult.memory) {
			Platform::WriteEntireFile("test.out", readFileResult.memory, readFileResult.memorySize);
			Platform::FreeFileMemory(readFileResult.memory);
		}

		if (input.moveUp.isEndedPressed)
			gameState->greenOffset--;
		if (input.moveDown.isEndedPressed)
			gameState->greenOffset++;
		if (input.moveRight.isEndedPressed)
			gameState->blueOffset++;
		if (input.moveLeft.isEndedPressed)
			gameState->blueOffset--;

		RenderGradient(gameState, screenBuffer);
	};

	static void RenderGradient(const GameState* gameState, ScreenBuffer& screenBuffer) {
		for (u32 y = 0; y < screenBuffer.height; y++) {
			for (u32 x = 0; x < screenBuffer.width; x++) {
				*screenBuffer.memory++ = {
					.blue = (u8)((x + gameState->blueOffset) & UINT8_MAX),
					.green = (u8)((y + gameState->greenOffset) & UINT8_MAX),
				};
			}
		}
	};

	static void GetSoundSamples(Memory& memory, SoundBuffer& soundBuffer) {
		GameState* gameState = (GameState*)memory.permanentStorage;

		u32 frequency = 261;
		f32 volume = 5000.0f;

		f32 samplesPerWavePeriod = (f32)(soundBuffer.samplesPerSecond / frequency);
		SoundSample* soundSample = soundBuffer.samples;
		gameState->tSine = std::fmod(gameState->tSine, 2.0f * pi32);

		for (u32 i = 0; i < soundBuffer.samplesToWrite; i++) {
			s16 sampleValue = (s16)(std::sinf(gameState->tSine) * volume);
			gameState->tSine += 2.0f * pi32 / samplesPerWavePeriod;
			*soundSample++ = {
				.left = sampleValue,
				.right = sampleValue
			};
		}
	}
}