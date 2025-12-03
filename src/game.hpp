#pragma once

#include "globals.hpp"

namespace Game {
	struct GameState {
		f32 tSine;
		u8 greenOffset, blueOffset;
	};

	struct ButtonState {
		u32 transitionsCount;
		bool isEndedPressed;
	};

	struct Input {
		ButtonState start, back;
		ButtonState leftShoulder, rightShoulder;
		ButtonState moveUp, moveDown, moveLeft, moveRight;
		ButtonState actionUp, actionDown, actionLeft, actionRight;

		void ResetTransitionsCount() {
			start.transitionsCount = 0;
			back.transitionsCount = 0;

			leftShoulder.transitionsCount = 0;
			rightShoulder.transitionsCount = 0;

			moveUp.transitionsCount = 0;
			moveDown.transitionsCount = 0;
			moveLeft.transitionsCount = 0;
			moveRight.transitionsCount = 0;

			actionUp.transitionsCount = 0;
			actionDown.transitionsCount = 0;
			actionLeft.transitionsCount = 0;
			actionRight.transitionsCount = 0;
		}
	};

	struct Memory {
		size_t permanentStorageSize;
		size_t transientStorageSize;
		std::byte* permanentStorage;
		std::byte* transientStorage;
		bool isInitialized;
	};

	struct ScreenPixel {
		u8 blue, green, red, padding;
	};

	struct ScreenBuffer {
		u32 width, height;
		ScreenPixel* memory;
	};

	struct SoundSample {
		s16 left, right;
	};

	struct SoundBuffer {
		u32 samplesPerSecond;
		u32 samplesToWrite;
		SoundSample* samples;
	};

	static void UpdateAndRender(Memory& memory, const Input& input, ScreenBuffer& screenBuffer);
	// GetSoundSamples should be fast, something like <= 1ms
	static void GetSoundSamples(Memory& memory, SoundBuffer& soundBuffer);
	static void RenderGradient(const GameState* gameState, ScreenBuffer& screenBuffer);
}