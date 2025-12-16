#include "game.cpp"
#include "win32_handmade.hpp"

static const u32 INITIAL_WINDOW_WIDTH = 1280;
static const u32 INITIAL_WINDOW_HEIGHT = 720;
static const u32 TARGET_UPDATE_FREQUENCY = 30;
static const f32 TARGET_SECONDS_PER_FRAME = 1.0f / (f32)TARGET_UPDATE_FREQUENCY;

static bool globalIsAppRunning = true;
static bool globalIsPause = false;
static u64 globalPerformanceFrequency = 0;
// global because of MainWindowCallback
static Screen globalScreen = {
	.bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(globalScreen.bitmapInfo.bmiHeader),
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	}
};

// TODO: use NULL instead of 0 if it is not a meaningful number
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
	static_assert(DEV_MODE || !SLOW_MODE);
	
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	bool sleepIsGranular = timeBeginPeriod(1) == TIMERR_NOERROR; // set Windows scheduler granularity in ms

	WNDCLASSA windowClass = {
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = MainWindowCallback,
		.hInstance = hInstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};
	RegisterClassA(&windowClass);
	HWND window = CreateWindowExA(
		0, windowClass.lpszClassName, "Handmade Hero", WS_TILEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT,
		0, 0, hInstance, 0
	);
	HDC deviceContext = GetDC(window);
	ResizeScreenBuffer(globalScreen, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);

	// TODO: platformSound
	Sound sound = {
		.waveFormat = {
			.wFormatTag = WAVE_FORMAT_PCM,
			.nChannels = 2,
			.nSamplesPerSec = 48000,
			.nAvgBytesPerSec = sizeof(Game::SoundSample) * sound.waveFormat.nSamplesPerSec,
			.nBlockAlign = sizeof(Game::SoundSample),
			.wBitsPerSample = sizeof(Game::SoundSample) / sound.waveFormat.nChannels * 8,
		},
		.bytesPerFrame = sound.waveFormat.nAvgBytesPerSec / TARGET_UPDATE_FREQUENCY,
		.safetyBytes = sound.bytesPerFrame / 3,
	};
	InitDirectSound(window, sound);
	Game::SoundSample* gameSoundMemory = (Game::SoundSample*)VirtualAlloc(0, sound.getBufferSize(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (sound.buffer) {
		ClearSoundBuffer(sound);
		// TODO: address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
		// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
		sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
	}
	Debug::Marker debugMarkersArray[TARGET_UPDATE_FREQUENCY - 1] = {};
	size_t debugMarkersIndex = 0;

	void* gameMemoryBaseAddress = 0;
	if constexpr (DEV_MODE && INTPTR_MAX == INT64_MAX) {
		gameMemoryBaseAddress = (void*)1024_GB;
	}
	size_t gameMemorySize = 1_GB;
	std::byte* gameMemoryStorage = (std::byte*)VirtualAlloc(gameMemoryBaseAddress, gameMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Game::Memory gameMemory = {
		.permanentStorageSize = 64_MB,
		.transientStorageSize = gameMemorySize - gameMemory.permanentStorageSize,
		.permanentStorage = gameMemoryStorage,
		.transientStorage = gameMemory.permanentStorage + gameMemory.permanentStorageSize
	};

	LoadXInputLibrary();
	Game::Input gameInput = {};

	LARGE_INTEGER performanceFrequencyResult;
	QueryPerformanceFrequency(&performanceFrequencyResult);
	globalPerformanceFrequency = (u64)performanceFrequencyResult.QuadPart;
	u64 flipWallClock = GetWallClock();
	u64 flipCycleCounter = __rdtsc();

	while (globalIsAppRunning) {
		gameInput.ResetTransitionsCount();
		ProcessPendingMessages(gameInput.controllers[0]);
		ProcessGamepadInput(gameInput.controllers[1]);

		if constexpr (DEV_MODE) {
			if (globalIsPause) {
				YieldProcessor();
				flipWallClock = GetWallClock();
				flipCycleCounter = __rdtsc();
				continue;
			};
		}

		Game::ScreenBuffer gameScreenBuffer = {
			.width = globalScreen.getWidth(),
			.height = globalScreen.getHeight(),
			.memory = globalScreen.memory,
		};

		Game::UpdateAndRender(gameMemory, gameInput, gameScreenBuffer);
		CalcRequiredSoundOutput(sound, flipWallClock, debugMarkersArray, debugMarkersIndex);

		Game::SoundBuffer gameSoundBuffer = {
			.samplesPerSecond = sound.waveFormat.nSamplesPerSec,
			.samplesToWrite = sound.outputByteCount / sound.waveFormat.nBlockAlign,
			.samples = gameSoundMemory,
		};

		Game::GetSoundSamples(gameMemory, gameSoundBuffer);
		FillSoundBuffer(gameSoundBuffer, sound);

		f32 frameSecondsElapsed = GetSecondsElapsed(flipWallClock);
		if (sleepIsGranular && frameSecondsElapsed < TARGET_SECONDS_PER_FRAME) {
			f32 sleepMs = 1000.0f * (TARGET_SECONDS_PER_FRAME - frameSecondsElapsed);
			if (sleepMs > 0) Sleep((DWORD)sleepMs);
		}
		frameSecondsElapsed = GetSecondsElapsed(flipWallClock);
		while (frameSecondsElapsed < TARGET_SECONDS_PER_FRAME) {
			YieldProcessor();
			frameSecondsElapsed = GetSecondsElapsed(flipWallClock);
		}
		flipWallClock = GetWallClock();
		flipCycleCounter = __rdtsc();

		if constexpr (DEV_MODE) {
			bool isCursorsRecorded = sound.buffer && SUCCEEDED(sound.buffer->GetCurrentPosition(
				&debugMarkersArray[debugMarkersIndex].flipPlayCursor, NULL
			));
			if (isCursorsRecorded) {
				Debug::SoundSyncDisplay(
					globalScreen, sound,
					debugMarkersArray, ArrayCount(debugMarkersArray), debugMarkersIndex
				);
			}

			// // TODO: log this instead of assert
			// size_t debugMarkersIndexUnwrapped = debugMarkersIndex == 0 ? ArrayCount(debugMarkersArray) : debugMarkersIndex;
			// Debug::Marker prevMarker = debugMarkersArray[debugMarkersIndexUnwrapped - 1];
			// DWORD prevSoundFilledCursor = (prevMarker.outputLocation + prevMarker.outputByteCount) % sound.getBufferSize();
			// // compare with half buffer size to account for the fact that prevSoundFilledCursor may be wrapped
			// assert(sound.playCursor <= prevSoundFilledCursor || sound.playCursor > prevSoundFilledCursor + sound.getBufferSize() / 2);
			
			// f32 millisecondsPerFrame = 1000 * frameSecondsElapsed;
			// u64 cycleCounterElapsed = __rdtsc() - flipCycleCounter;
			char outputBuffer[256];
			sprintf_s(outputBuffer, "frame ms: %.2f\n", frameSecondsElapsed * 1000);
			OutputDebugStringA(outputBuffer);

			debugMarkersIndex = (debugMarkersIndex + 1) % ArrayCount(debugMarkersArray);
		}

		DisplayScreenBuffer(window, deviceContext, globalScreen);
	}

	return 0;
}

static LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);
			DisplayScreenBuffer(window, deviceContext, globalScreen);
			EndPaint(window, &paint);
		} break;
		case WM_CLOSE:
		case WM_DESTROY: {
			globalIsAppRunning = false;
		} break;
		case WM_KEYUP:
		case WM_KEYDOWN: {
			assert(!"Keyboard input came in through non-dispatched message!");
		} break;
		default:
			return DefWindowProcA(window, message, wParam, lParam);
	}

	return 0;
}

static void DisplayScreenBuffer(HWND window, HDC deviceContext, const Screen& screen) {
	RECT clientRect;
	GetClientRect(window, &clientRect);

	int destWidth = clientRect.right - clientRect.left;
	int destHeight = clientRect.bottom - clientRect.top;
	int srcWidth = (int)screen.getWidth();
	int srcHeight = (int)screen.getHeight();

	StretchDIBits(deviceContext,
		0, 0, destWidth, destHeight,
		0, 0, srcWidth, srcHeight,
		screen.memory, &screen.bitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

static void ResizeScreenBuffer(Screen& screen, u32 width, u32 height) {
	if (screen.memory) {
		VirtualFree(screen.memory, 0, MEM_RELEASE);
	}
	
	screen.setWidth(width);
	screen.setHeight(height);
	size_t screenMemorySize = width * height * sizeof(*screen.memory);
	screen.memory = (Game::ScreenPixel*)VirtualAlloc(0, screenMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void InitDirectSound(HWND window, Sound& sound) {
	HMODULE directSoundLibrary = LoadLibraryA("dsound.dll");
	if (!directSoundLibrary) return;

	typedef HRESULT WINAPI DirectSoundCreateType(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
	auto directSoundCreate = (DirectSoundCreateType*)GetProcAddress(directSoundLibrary, "DirectSoundCreate");
	if (!directSoundCreate) return;

	IDirectSound* directSound;
	if (!SUCCEEDED(directSoundCreate(0, &directSound, 0))) return;
	if (!SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY))) return;

	DSBUFFERDESC primaryBufferDesc = {
		.dwSize = sizeof(primaryBufferDesc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};

	IDirectSoundBuffer* primaryBuffer;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&primaryBufferDesc, &primaryBuffer, 0))) return;
	if (!SUCCEEDED(primaryBuffer->SetFormat(&sound.waveFormat))) return;

	DSBUFFERDESC soundBufferDesc = {
		.dwSize = sizeof(soundBufferDesc),
		.dwBufferBytes = sound.getBufferSize(),
		.lpwfxFormat = &sound.waveFormat
	};

	directSound->CreateSoundBuffer(&soundBufferDesc, &sound.buffer, 0);
}

static void CalcRequiredSoundOutput(Sound& sound, u64 flipWallClock, Debug::Marker* debugMarkersArray, size_t debugMarkersIndex) {
	// Here is how sound output computation works:
	// We define a safety value that is the number of samples we think our game update loop may vary by
	// (let's say up to 2ms). When we wake up to write audio, we will look and see what the play cursor
	// position is and we will forecast ahead where we think the play cursor will be on the next frame boundary.
	// We will then look to see if the write cursor is before that by at least our safety value.
	// If it is, the target fill position is that frame boundary plus one frame.
	// This gives us perfect audio sync in the case of a card that has low enough latency.
	// If the write cursor is after that safery margin, then we assume we can never sync the audio perfectly,
	// so we will write one frame's worth of audio plus the safety margin's worth of guard samples.

	f32 fromFlipToAudioSeconds = GetSecondsElapsed(flipWallClock);
	if (!sound.buffer || !SUCCEEDED(sound.buffer->GetCurrentPosition(&sound.playCursor, &sound.writeCursor))) {
		sound.runningSampleIndex = sound.writeCursor / sound.waveFormat.nBlockAlign;
	}

	DWORD expectedBytesUntilFlip = sound.bytesPerFrame - (DWORD)((f32)sound.waveFormat.nAvgBytesPerSec * fromFlipToAudioSeconds);
	DWORD expectedFlipPlayCursorUnwrapped = sound.playCursor + expectedBytesUntilFlip;
	DWORD writeCursorUnwrapped = sound.playCursor < sound.writeCursor
		? sound.writeCursor
		: sound.writeCursor + sound.getBufferSize();

	bool isLowLatencySound = (writeCursorUnwrapped + sound.safetyBytes) < expectedFlipPlayCursorUnwrapped;	
	DWORD targetCursorUnwrapped = isLowLatencySound
		? expectedFlipPlayCursorUnwrapped + sound.bytesPerFrame
		: writeCursorUnwrapped + sound.bytesPerFrame + sound.safetyBytes;

	DWORD targetCursor = targetCursorUnwrapped % sound.getBufferSize();
	sound.outputLocation = sound.runningSampleIndex * sound.waveFormat.nBlockAlign % sound.getBufferSize();
	sound.outputByteCount = sound.outputLocation < targetCursor ? 0 : sound.getBufferSize();
	sound.outputByteCount += targetCursor - sound.outputLocation;

	if constexpr (DEV_MODE) {
		debugMarkersArray[debugMarkersIndex].outputPlayCursor = sound.playCursor;
		debugMarkersArray[debugMarkersIndex].outputWriteCursor = sound.writeCursor;
		debugMarkersArray[debugMarkersIndex].outputLocation = sound.outputLocation;
		debugMarkersArray[debugMarkersIndex].outputByteCount = sound.outputByteCount;
		debugMarkersArray[debugMarkersIndex].expectedFlipPlayCursor = expectedFlipPlayCursorUnwrapped % sound.getBufferSize();
	}
}

static void ClearSoundBuffer(Sound& sound) {
	void *region1, *region2; DWORD region1Size, region2Size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(0, sound.getBufferSize(), &region1, &region1Size, &region2, &region2Size, 0))) {
		return;
	}

	std::memset(region1, 0, region1Size);
	std::memset(region2, 0, region2Size);
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void FillSoundBuffer(const Game::SoundBuffer& source, Sound& sound) {
	void *region1, *region2; DWORD region1Size, region2Size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(sound.outputLocation, sound.outputByteCount, &region1, &region1Size, &region2, &region2Size, 0))) {
		return;
	}

	u32 sampleSize = sizeof(*(source.samples));
	u32 region1SizeInSamples = region1Size / sampleSize;
	u32 region2SizeInSamples = region2Size / sampleSize;
	sound.runningSampleIndex += region1SizeInSamples + region2SizeInSamples;

	std::memcpy(region1, source.samples, region1Size);
	std::memcpy(region2, source.samples + region1SizeInSamples, region2Size);
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ProcessPendingMessages(Game::Controller& controller) {
	MSG message;
	while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
		switch (message.message) {
			case WM_QUIT: {
				globalIsAppRunning = false;
			} break;
			case WM_KEYUP:
			case WM_KEYDOWN: {
				bool isKeyPressed = !(message.lParam & (1u << 31));
				bool wasKeyPressed = message.lParam & (1u << 30);
				if (isKeyPressed == wasKeyPressed) continue;

				if constexpr (DEV_MODE) {
					if (message.wParam == 'P' && message.message == WM_KEYDOWN) {
						globalIsPause = !globalIsPause;
					}
				}

				Game::ButtonState* buttonState;
				switch (message.wParam) {
					case VK_RETURN: buttonState = &controller.start; break;
					case VK_ESCAPE: buttonState = &controller.back; break;
					case VK_UP: 	buttonState = &controller.moveUp; break;
					case VK_DOWN: 	buttonState = &controller.moveDown; break;
					case VK_LEFT: 	buttonState = &controller.moveLeft; break;
					case VK_RIGHT: 	buttonState = &controller.moveRight; break;
					case 'W': 		buttonState = &controller.actionUp; break;
					case 'S': 		buttonState = &controller.actionDown; break;
					case 'A': 		buttonState = &controller.actionLeft; break;
					case 'D': 		buttonState = &controller.actionRight; break;
					case 'Q': 		buttonState = &controller.leftShoulder; break;
					case 'E': 		buttonState = &controller.rightShoulder; break;
					default: continue;
				}
				buttonState->isPressed = isKeyPressed;
				buttonState->transitionsCount++;
			} break;
			default: {
				TranslateMessage(&message);
				DispatchMessageA(&message);
			}
		}
	}
}

static void LoadXInputLibrary() {
	HMODULE xInputLibrary = LoadLibraryA("xinput1_3.dll");
	if (!xInputLibrary) return;
	
	XInputGetState = (XInputGetStateType*) GetProcAddress(xInputLibrary, "XInputGetState");
	XInputSetState = (XInputSetStateType*) GetProcAddress(xInputLibrary, "XInputSetState");
}

static void ProcessGamepadInput(Game::Controller& controller) {
	XINPUT_STATE state;
	XINPUT_GAMEPAD& gamepad = state.Gamepad;
	if (XInputGetState(0, &state)) return;
	
	controller.isAnalog = true;
	controller.startX = controller.endX;
	controller.startY = controller.endY;
	controller.endX = controller.minX = controller.maxX = GetNormalizedStickValue(gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	controller.endY = controller.minY = controller.maxY = GetNormalizedStickValue(gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

	ProcessGamepadButton(controller.start, gamepad.wButtons & XINPUT_GAMEPAD_START);
	ProcessGamepadButton(controller.back, gamepad.wButtons & XINPUT_GAMEPAD_BACK);
	ProcessGamepadButton(controller.leftShoulder, gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
	ProcessGamepadButton(controller.rightShoulder, gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

	ProcessGamepadButton(controller.moveUp, gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
	ProcessGamepadButton(controller.moveDown, gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
	ProcessGamepadButton(controller.moveLeft, gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
	ProcessGamepadButton(controller.moveRight, gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

	ProcessGamepadButton(controller.actionUp, gamepad.wButtons & XINPUT_GAMEPAD_A);
	ProcessGamepadButton(controller.actionDown, gamepad.wButtons & XINPUT_GAMEPAD_B);
	ProcessGamepadButton(controller.actionLeft, gamepad.wButtons & XINPUT_GAMEPAD_X);
	ProcessGamepadButton(controller.actionRight, gamepad.wButtons & XINPUT_GAMEPAD_Y);
}

static inline f32 GetNormalizedStickValue(SHORT value, SHORT deadzone) {
	return (-deadzone <= value && value < 0) || (0 < value && value <= deadzone) ? 0 : value / 32768.0f;
}

static inline void ProcessGamepadButton(Game::ButtonState& state, bool isPressed) {
	state.transitionsCount += isPressed != state.isPressed;
	state.isPressed = isPressed;
}

static inline u64 GetWallClock() {
	LARGE_INTEGER performanceCounterResult;
	QueryPerformanceCounter(&performanceCounterResult);
	return (u64)performanceCounterResult.QuadPart;
}

static inline f32 GetSecondsElapsed(u64 start) {
	return (f32)(GetWallClock() - start) / (f32)globalPerformanceFrequency;
}

namespace Platform {
	static ReadEntireFileResult ReadEntireFileSync(const char* fileName) {
		ReadEntireFileResult result = {};
		u32 memorySize = 0;
		void* memory = nullptr;

		// seems like this handle don't need to be closed
		HANDLE heapHandle = GetProcessHeap();
		if (!heapHandle) return result;

		HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		if (fileHandle == INVALID_HANDLE_VALUE) return result;

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(fileHandle, &fileSize)) goto close_file_handle;

		memorySize = SafeTruncateToU32(fileSize.QuadPart);
		memory = HeapAlloc(heapHandle, 0, memorySize);
		if (!memory) goto close_file_handle;

		DWORD bytesRead;
		if (!ReadFile(fileHandle, memory, memorySize, &bytesRead, 0) || (bytesRead != memorySize)) {
			HeapFree(heapHandle, 0, memory);
			goto close_file_handle;
		}

		result.memorySize = memorySize;
		result.memory = memory;

		close_file_handle:
			CloseHandle(fileHandle);
			return result;
	}
	static bool WriteEntireFileSync(const char* fileName, const void* memory, u32 memorySize) {
		bool result = false;

		HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		if (fileHandle == INVALID_HANDLE_VALUE) return result;

		DWORD bytesWritten;
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0) && (bytesWritten == memorySize)) {
			result = true;
		}

		CloseHandle(fileHandle);
		return result;
	}
	static void FreeFileMemory(void* memory) {
		if (!memory) return;

		HANDLE heapHandle = GetProcessHeap();
		if (!heapHandle) return;

		HeapFree(heapHandle, 0, memory);
		memory = nullptr;
	}
}

namespace Debug {
	static void SoundSyncDisplay(
		Screen& screen, const Sound& sound,
		const Marker* markers, size_t markersCount, size_t currentMarkerIndex) {

		f32 horizontalScaling = (f32)screen.getWidth() / (f32)sound.getBufferSize();
		Marker currentMarker = markers[currentMarkerIndex];

		u32 expectedFlipPlayCursorX = (u32)((f32)currentMarker.expectedFlipPlayCursor * horizontalScaling);
		DrawVertical(screen, expectedFlipPlayCursorX, 0, screen.getHeight(), 0xffffff00);

		for (size_t i = 0; i < markersCount; i++) {
			u32 top = 0;
			u32 bottom = screen.getHeight() * 1/4;
			u32 historicOutputPlayCursorX = (u32)((f32)markers[i].outputPlayCursor * horizontalScaling);
			u32 historicOutputWriteCursorX = (u32)((f32)markers[i].outputWriteCursor * horizontalScaling);
			DrawVertical(screen, historicOutputPlayCursorX, top, bottom, 0x00ffffff);
			DrawVertical(screen, historicOutputWriteCursorX, top, bottom, 0x00ff0000);
		}
		{
			u32 top = screen.getHeight() * 1/4;
			u32 bottom = screen.getHeight() * 2/4;
			u32 outputPlayCursorX = (u32)((f32)currentMarker.outputPlayCursor * horizontalScaling);
			u32 outputWriteCursorX = (u32)((f32)currentMarker.outputWriteCursor * horizontalScaling);
			DrawVertical(screen, outputPlayCursorX, top, bottom, 0x00ffffff);
			DrawVertical(screen, outputWriteCursorX, top, bottom, 0x00ff0000);
		}
		{
			u32 top = screen.getHeight() * 2/4;
			u32 bottom = screen.getHeight() * 3/4;
			u32 outputLocationX = (u32)((f32)currentMarker.outputLocation * horizontalScaling);
			u32 outputByteCountX = (u32)((f32)currentMarker.outputByteCount * horizontalScaling);
			DrawVertical(screen, outputLocationX, top, bottom, 0x00ffffff);
			DrawVertical(screen, (outputLocationX + outputByteCountX) % screen.getWidth(), top, bottom, 0x00ff0000);
		}
		{
			u32 top = screen.getHeight() * 3/4;
			u32 bottom = screen.getHeight();
			u32 flipPlayCursorX = (u32)((f32)currentMarker.flipPlayCursor * horizontalScaling);
			u32 cursorGranularityBytesX = (u32)(1920.0f * horizontalScaling);
			DrawVertical(screen, (flipPlayCursorX - cursorGranularityBytesX / 2) % screen.getWidth(), top, bottom, 0x00ffffff);
			DrawVertical(screen, (flipPlayCursorX + cursorGranularityBytesX / 2) % screen.getWidth(), top, bottom, 0x00ffffff);
		}
	}
	
	static void DrawVertical(Screen& screen, u32 x, u32 top, u32 bottom, u32 color) {
		Game::ScreenPixel* pixel = screen.memory + top * screen.getWidth() + x;

		for (u32 y = top; y < bottom; y++) {
			// TODO: maybe add u32 constructor
			*pixel = *(Game::ScreenPixel*)&color;
			pixel += screen.getWidth();
		}
	}
}