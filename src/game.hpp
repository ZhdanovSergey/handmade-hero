#pragma once

#include "globals.hpp"

namespace Game {
	struct GameState {
		f64 tSine;
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
		std::byte* permanentStorage;
		std::byte* transientStorage;
		bool isInitialized;
	};

	struct ScreenPixel {
		u8 blue;
		u8 green;
		u8 red;
		u8 padding;
	};

	struct ScreenBuffer {
		u32 width;
		u32 height;
		ScreenPixel* memory;
	};

	struct SoundSample {
		s16 left;
		s16 right;
	};

	struct SoundBuffer {
		u32 samplesPerSecond;
		u32 samplesToWrite;
		SoundSample* samples;
	};

	static void UpdateAndRender(const Input* input, Memory* memory, ScreenBuffer* screenBuffer, SoundBuffer* soundBuffer);
	static void RenderGradient(const GameState* gameState, ScreenBuffer* screenBuffer);
	static void OutputSound(GameState* gameState, SoundBuffer* soundBuffer);
}