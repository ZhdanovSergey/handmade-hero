#pragma once

#include "globals.hpp"

namespace Game {
	struct GameState {
		f32 tSine;
		u32 greenOffset;
		u32 blueOffset;
	};

	struct ButtonState {
		u32 transitionsCount;
		bool isEndedPressed;
	};

	struct Input {
		ButtonState start;
		ButtonState back;

		ButtonState moveUp;
		ButtonState moveDown;
		ButtonState moveLeft;
		ButtonState moveRight;

		ButtonState actionUp;
		ButtonState actionDown;
		ButtonState actionLeft;
		ButtonState actionRight;

		ButtonState leftShoulder;
		ButtonState rightShoulder;

		void ResetTransitionsCount() {
			start.transitionsCount = 0;
			back.transitionsCount = 0;

			moveUp.transitionsCount = 0;
			moveDown.transitionsCount = 0;
			moveLeft.transitionsCount = 0;
			moveRight.transitionsCount = 0;

			actionUp.transitionsCount = 0;
			actionDown.transitionsCount = 0;
			actionLeft.transitionsCount = 0;
			actionRight.transitionsCount = 0;

			leftShoulder.transitionsCount = 0;
			rightShoulder.transitionsCount = 0;
		}
	};

	struct Memory {
		size_t permanentStorageSize;
		size_t transientStorageSize;
		void* permanentStorage;
		void* transientStorage;
		bool isInitialized;
	};

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

	static void UpdateAndRender(Input* input, Memory* memory, ScreenBuffer* screenBuffer, SoundBuffer* soundBuffer);
	static void RenderGradient(GameState* gameState, ScreenBuffer* screenBuffer);
	static void OutputSound(GameState* gameState, SoundBuffer* soundBuffer);
}