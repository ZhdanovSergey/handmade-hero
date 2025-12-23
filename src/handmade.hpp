#pragma once

#include "globals.hpp"

namespace Game {
	struct GameState {
		f32 tSine;
		u32 toneHz;
		u32 greenOffset, blueOffset;
	};

	struct ButtonState {
		bool isPressed;
		u32 transitionsCount;
	};

	struct Controller {
		bool isAnalog;

		f32 startX, startY;
		f32 endX, endY;
		f32 minX, minY;
		f32 maxX, maxY;

		ButtonState start, back;
		ButtonState leftShoulder, rightShoulder;
		ButtonState moveUp, moveDown, moveLeft, moveRight;
		ButtonState actionUp, actionDown, actionLeft, actionRight;
	};

	struct Input {
		Controller controllers[2];

		void ResetTransitionsCount() {
			for (auto& controller : controllers) {
				controller.start.transitionsCount = 0;
				controller.back.transitionsCount = 0;

				controller.leftShoulder.transitionsCount = 0;
				controller.rightShoulder.transitionsCount = 0;

				controller.moveUp.transitionsCount = 0;
				controller.moveDown.transitionsCount = 0;
				controller.moveLeft.transitionsCount = 0;
				controller.moveRight.transitionsCount = 0;

				controller.actionUp.transitionsCount = 0;
				controller.actionDown.transitionsCount = 0;
				controller.actionLeft.transitionsCount = 0;
				controller.actionRight.transitionsCount = 0;
			}
		}
	};

	struct Memory {
		bool isInitialized;
		size_t permanentStorageSize;
		size_t transientStorageSize;
		std::byte* permanentStorage;
		std::byte* transientStorage;

    	Platform::ReadEntireFileSyncType* ReadEntireFileSync;
    	Platform::WriteEntireFileSyncType* WriteEntireFileSync;
    	Platform::FreeFileMemoryType* FreeFileMemory;
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

	typedef 	void UpdateAndRenderType(const Game::Input&, Game::Memory&, Game::ScreenBuffer&);
	extern "C"	void UpdateAndRender	(const Game::Input&, Game::Memory&, Game::ScreenBuffer&);
				void UpdateAndRenderStub(const Game::Input&, Game::Memory&, Game::ScreenBuffer&){};

	// GetSoundSamples должен быть быстрым, не больше 1ms
	typedef		void GetSoundSamplesType(Game::Memory&, Game::SoundBuffer&);
	extern "C"	void GetSoundSamples	(Game::Memory&, Game::SoundBuffer&);
				void GetSoundSamplesStub(Game::Memory&, Game::SoundBuffer&){};

	static void RenderGradient(const GameState* gameState, ScreenBuffer& screenBuffer);
}