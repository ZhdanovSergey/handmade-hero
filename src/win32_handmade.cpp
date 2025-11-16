#include "game.cpp"
#include "win32_handmade.hpp"

static bool globalIsAppRunning = true;
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

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
	static_assert(HANDMADE_DEV || !HANDMADE_SLOW);

	const u32 INITIAL_WINDOW_WIDTH = 1280;
	const u32 INITIAL_WINDOW_HEIGHT = 720;
	const u32 TARGET_UPDATE_FREQUENCY = 30;
	const f32 TARGET_SECONDS_PER_FRAME = 1.0f / (f32)TARGET_UPDATE_FREQUENCY;

	LARGE_INTEGER performanceFrequencyResult;
	QueryPerformanceFrequency(&performanceFrequencyResult);
	globalPerformanceFrequency = (u64)performanceFrequencyResult.QuadPart;

	// set Windows scheduler granularity in ms
	bool sleepIsGranular = timeBeginPeriod(1) == TIMERR_NOERROR;

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

	Sound sound = {
		.waveFormat = {
			.wFormatTag = WAVE_FORMAT_PCM,
			.nChannels = 2,
			.nSamplesPerSec = 48000,
			.nAvgBytesPerSec = sizeof(Game::SoundSample) * sound.waveFormat.nSamplesPerSec,
			.nBlockAlign = sizeof(Game::SoundSample),
			.wBitsPerSample = sizeof(Game::SoundSample) / sound.waveFormat.nChannels * 8,
		},
		.latencySamples = sound.waveFormat.nSamplesPerSec / TARGET_UPDATE_FREQUENCY,
	};

	// minus 1 for better visualization
	Debug::SoundCursors debugSoundCursorsArray[TARGET_UPDATE_FREQUENCY - 1] = {};
	size_t debugSoundCursorsIndex = 0;

	InitDirectSound(window, sound);
	Game::SoundSample* gameSoundSamples = (Game::SoundSample*)VirtualAlloc(0, sound.getBufferSize(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	
	if (sound.buffer) {
		ClearSoundBuffer(sound);
		// TODO: address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
		// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
		sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	void* gameMemoryBaseAddress = 0;
	if constexpr (HANDMADE_DEV && INTPTR_MAX == INT64_MAX)
		gameMemoryBaseAddress = (void*)1024_GB;

	size_t gameMemorySize = 1_GB;
	std::byte* gameMemoryStorage = (std::byte*)VirtualAlloc(gameMemoryBaseAddress, gameMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	Game::Memory gameMemory = {
		.permanentStorageSize = 64_MB,
		.transientStorageSize = gameMemorySize - gameMemory.permanentStorageSize,
		.permanentStorage = gameMemoryStorage,
		.transientStorage = gameMemory.permanentStorage + gameMemory.permanentStorageSize
	};

	Game::Input gameInput = {};

	u64 startWallClock = GetWallClock();
	u64 startCycleCounter = __rdtsc();

	while (globalIsAppRunning) {
		ProcessPendingMessages(gameInput);

		// Here is how sound output computation works:
		// We define a safety value that is the number of samples we think our game update loop may vary by
		// (let's say up to 2ms). When we wake up to write audio, we will look and see what the play cursor
		// position is and we will forecast ahead where we think the play cursor will be on the next frame boundary.
		// We will then look to see if the write cursor is before that by at least our safety value.
		// If it is, the target fill position is that frame boundary plus one frame.
		// This gives us perfect audio sync in the case of a card that has low enough latency.
		// If the write cursor is after that safery margin, then we assume we can never sync the audio perfectly,
		// so we will write one frame's worth of audio plus the safety margin's worth of guard samples.

		if (sound.buffer && sound.isValid) {
			DWORD targetCursor = (sound.playCursor + sound.latencySamples * sound.waveFormat.nBlockAlign) % sound.getBufferSize();
			sound.lockCursor = (sound.runningSampleIndex * sound.waveFormat.nBlockAlign) % sound.getBufferSize();
			sound.bytesToWrite = sound.lockCursor > targetCursor ? sound.getBufferSize() : 0;
			sound.bytesToWrite += targetCursor - sound.lockCursor;
		}

		sound.isValid = sound.buffer && SUCCEEDED(sound.buffer->GetCurrentPosition(&sound.playCursor, &sound.writeCursor));
		if (!sound.isValid) {
			sound.runningSampleIndex = sound.writeCursor / sound.waveFormat.nBlockAlign;
		} else {
			DWORD unwrappedWriteCursor = sound.playCursor < sound.writeCursor
				? sound.writeCursor
				: sound.writeCursor + sound.getBufferSize();
			sound.latencyBytes = unwrappedWriteCursor - sound.playCursor;
		}

		Game::SoundBuffer gameSoundBuffer = {
			.samplesPerSecond = sound.waveFormat.nSamplesPerSec,
			.samplesToWrite = sound.bytesToWrite / sound.waveFormat.nBlockAlign,
			.samples = gameSoundSamples,
		};

		Game::ScreenBuffer gameScreenBuffer = {
			.width = globalScreen.getWidth(),
			.height = globalScreen.getHeight(),
			.memory = globalScreen.memory,
		};

		Game::UpdateAndRender(gameInput, gameMemory, gameScreenBuffer, gameSoundBuffer);

		f32 frameSecondsElapsed = GetSecondsElapsed(startWallClock);
		if (sleepIsGranular && frameSecondsElapsed < TARGET_SECONDS_PER_FRAME) {
			DWORD sleepErrorMs = 0;
			DWORD sleepMs = (DWORD)(1000.0f * (TARGET_SECONDS_PER_FRAME - frameSecondsElapsed));
			if (sleepMs > sleepErrorMs) Sleep(sleepMs - sleepErrorMs);
		}

		frameSecondsElapsed = GetSecondsElapsed(startWallClock);
		// TODO: find a way to lock frame rate more reliably? It shouldn`t be assert anyway
		// if (globalIsAppRunning) assert(frameSecondsElapsed < targetSecondsPerFrame);

		while (frameSecondsElapsed < TARGET_SECONDS_PER_FRAME)
			frameSecondsElapsed = GetSecondsElapsed(startWallClock);

		if constexpr (HANDMADE_DEV) {
			debugSoundCursorsArray[debugSoundCursorsIndex] = {
				.playCursor = sound.playCursor,
				.writeCursor = sound.writeCursor
			};
			debugSoundCursorsIndex = (debugSoundCursorsIndex + 1) % ArrayCount(debugSoundCursorsArray);
			Debug::SyncDisplay(globalScreen, sound, debugSoundCursorsArray, ArrayCount(debugSoundCursorsArray));

			f32 millisecondsPerFrame = 1000 * frameSecondsElapsed;
			// f32 framesPerSecond = 1.0f / frameSecondsElapsed;
			// u64 endCycleCounter = __rdtsc();
			// u64 cycleCounterElapsed = endCycleCounter - startCycleCounter;
			// u64 megaCyclesPerFrame = cycleCounterElapsed / (1000 * 1000);
			char outputBuffer[256];
			sprintf_s(outputBuffer, "%.2f ms/f, latency ms: %f\n", millisecondsPerFrame, sound.getLatencySeconds() * 1000);
			OutputDebugStringA(outputBuffer);
		}
		
		startWallClock = GetWallClock();
		startCycleCounter = __rdtsc();

		if (sound.bytesToWrite)
			FillSoundBuffer(gameSoundBuffer, sound);

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
		case WM_KEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN: {
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
	if (screen.memory)
		VirtualFree(screen.memory, 0, MEM_RELEASE);

	screen.setWidth(width);
	screen.setHeight(height);
	size_t screenMemorySize = width * height * sizeof(*screen.memory);
	screen.memory = (Game::ScreenPixel*)VirtualAlloc(0, screenMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void InitDirectSound(HWND window, Sound& sound) {
	HMODULE directSoundLibrary = LoadLibraryA("dsound.dll");
	if (!directSoundLibrary) return;

	#pragma warning(suppress: 4191)
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

static void ClearSoundBuffer(Sound& sound) {
	void *region1, *region2; DWORD region1Size, region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(0, sound.getBufferSize(), &region1, &region1Size, &region2, &region2Size, 0)))
		return;

	std::memset(region1, 0, region1Size);
	std::memset(region2, 0, region2Size);
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void FillSoundBuffer(const Game::SoundBuffer& source, Sound& sound) {
	void *region1, *region2; DWORD region1Size, region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(sound.lockCursor, sound.bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0)))
		return;

	u32 sampleSize = sizeof(*(source.samples));
	u32 region1SizeInSamples = region1Size / sampleSize;
	u32 region2SizeInSamples = region2Size / sampleSize;
	sound.runningSampleIndex += region1SizeInSamples + region2SizeInSamples;

	std::memcpy(region1, source.samples, region1Size);
	std::memcpy(region2, source.samples + region1SizeInSamples, region2Size);
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ProcessPendingMessages(Game::Input& gameInput) {
	gameInput.ResetTransitionsCount();

	MSG message;
	while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
		switch (message.message) {
			case WM_QUIT: {
				globalIsAppRunning = false;
			} break;
			case WM_KEYUP:
			case WM_KEYDOWN:
			case WM_SYSKEYUP:
			case WM_SYSKEYDOWN: {
				Game::ButtonState* buttonState = nullptr;
				bool wasKeyPressed = message.lParam & (1u << 30);
				bool isKeyPressed = !(message.lParam & (1u << 31));
				if (wasKeyPressed == isKeyPressed) continue;

				switch (message.wParam) {
					case VK_RETURN: buttonState = &gameInput.start; break;
					case VK_ESCAPE: buttonState = &gameInput.back; break;
					case VK_UP: 	buttonState = &gameInput.moveUp; break;
					case VK_DOWN: 	buttonState = &gameInput.moveDown; break;
					case VK_LEFT: 	buttonState = &gameInput.moveLeft; break;
					case VK_RIGHT: 	buttonState = &gameInput.moveRight; break;
					case 'W': 		buttonState = &gameInput.actionUp; break;
					case 'S': 		buttonState = &gameInput.actionDown; break;
					case 'A': 		buttonState = &gameInput.actionLeft; break;
					case 'D': 		buttonState = &gameInput.actionRight; break;
					case 'Q': 		buttonState = &gameInput.leftShoulder; break;
					case 'E': 		buttonState = &gameInput.rightShoulder; break;
					default: continue;
				}

				buttonState->isEndedPressed = isKeyPressed;
				buttonState->transitionsCount++;
			} break;
			default: {
				TranslateMessage(&message);
				DispatchMessageA(&message);
			}
		}
	}
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
	static void FreeFileMemory(void* memory) {
		if (!memory) return;

		HANDLE heapHandle = GetProcessHeap();
		if (!heapHandle) return;

		HeapFree(heapHandle, 0, memory);
		memory = nullptr;
	}

	static ReadEntireFileResult ReadEntireFile(const char* fileName) {
		ReadEntireFileResult result = {};
		u32 memorySize = 0;
		void* memory = nullptr;

		// seems like this handle don't need to be closed
		HANDLE heapHandle = GetProcessHeap();
		if (!heapHandle)
			return result;

		HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return result;

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(fileHandle, &fileSize))
			goto close_file_handle;

		memorySize = SafeTruncateToU32(fileSize.QuadPart);
		memory = HeapAlloc(heapHandle, 0, memorySize);
		if (!memory)
			goto close_file_handle;

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

	static bool WriteEntireFile(const char* fileName, const void* memory, u32 memorySize) {
		bool result = false;

		HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return result;

		DWORD bytesWritten;
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0) && (bytesWritten == memorySize))
			result = true;

		CloseHandle(fileHandle);
		return result;
	}
}

namespace Debug {
	static void SyncDisplay(Screen& screen, const Sound& sound, const SoundCursors* soundCursors, size_t soundCursorsCount) {
		for (size_t i = 0; i < soundCursorsCount; i++) {
			u32 playCursorPosition = (u32)((f32)soundCursors[i].playCursor * (f32)screen.getWidth() / (f32)sound.getBufferSize());
			u32 writeCursorPosition = (u32)((f32)soundCursors[i].writeCursor * (f32)screen.getWidth() / (f32)sound.getBufferSize());
			DrawVertical(screen, playCursorPosition, 0, screen.getHeight(), 0x00ffffff);
			DrawVertical(screen, writeCursorPosition, 0, screen.getHeight(), 0x00ff0000);
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