#include "handmade.hpp"

namespace Game {
	extern "C" void UpdateAndRender(const Input& input, Memory& memory, ScreenBuffer& screenBuffer) {
		assert(sizeof(GameState) <= memory.permanentStorageSize);
		if (!memory.isInitialized) memory.isInitialized = true;
		GameState* gameState = (GameState*)memory.permanentStorage;

		for (auto& controller : input.controllers) {
			if (controller.moveUp.isPressed) 	gameState->greenOffset -= 10;
			if (controller.moveDown.isPressed) 	gameState->greenOffset += 10;
			if (controller.moveRight.isPressed)	gameState->blueOffset += 10;
			if (controller.moveLeft.isPressed)	gameState->blueOffset -= 10;
			
			gameState->toneHz = 256;
			if (controller.isAnalog) {
				gameState->toneHz += (u32)(256.0f * std::sqrtf(controller.endX * controller.endX + controller.endY * controller.endY));
				gameState->greenOffset -= (u32)(20.0f * controller.endY);
				gameState->blueOffset += (u32)(20.0f * controller.endX);
			}
		}

		RenderGradient(gameState, screenBuffer);
	};

	extern "C" void GetSoundSamples(Memory& memory, SoundBuffer& soundBuffer) {
		GameState* gameState = (GameState*)memory.permanentStorage;
		f32 volume = 5000.0f;

		f32 samplesPerWavePeriod = (f32)(soundBuffer.samplesPerSecond / gameState->toneHz);
		SoundSample* soundSample = soundBuffer.samples;

		for (u32 i = 0; i < soundBuffer.samplesToWrite; i++) {
			s16 sampleValue = (s16)(std::sinf(gameState->tSine) * volume);
			gameState->tSine += 2.0f * pi32 / samplesPerWavePeriod;
			*soundSample++ = {
				.left = sampleValue,
				.right = sampleValue
			};
		}
		gameState->tSine = std::fmod(gameState->tSine, 2.0f * pi32);
	}

	static void RenderGradient(const GameState* gameState, ScreenBuffer& screenBuffer) {
		for (u32 y = 0; y < screenBuffer.height; y++) {
			for (u32 x = 0; x < screenBuffer.width; x++) {
				*screenBuffer.memory++ = {
					.blue = (u8)(x + gameState->blueOffset),
					.green = (u8)(y + gameState->greenOffset),
				};
			}
		}
	};
}

// TODO: вынести в отдельный файл и избавиться от флага WIN32?
#if WIN32
	int __stdcall DllMain(_In_ void*, _In_ unsigned long, _In_ void*) { return 1; }
#endif