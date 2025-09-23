#include "globals.hpp"
#include "game.cpp"

#include <windows.h>
#include <dsound.h>

struct Screen {
	BITMAPINFO bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(bitmapInfo.bmiHeader),
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	};

	void* memory;

	void setWidth(u32 width) {
		bitmapInfo.bmiHeader.biWidth = (LONG)width;
	}

	u32 getWidth() {
		return (u32)bitmapInfo.bmiHeader.biWidth;
	}

	void setHeight(u32 height) {
		// biHeight is negative in order to top-left pixel been first in bitmap
		bitmapInfo.bmiHeader.biHeight = - (LONG)height;
	}

	u32 getHeight() {
		return (u32)std::abs(bitmapInfo.bmiHeader.biHeight);
	}

	size_t getMemorySize() {
		return bitmapInfo.bmiHeader.biBitCount / 8u * getWidth() * getHeight();
	}
};

struct Sound {
	WAVEFORMATEX waveFormat = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = 48000,
		.nAvgBytesPerSec = sizeof(s16) * waveFormat.nChannels * waveFormat.nSamplesPerSec,
		.nBlockAlign = sizeof(s16) * waveFormat.nChannels,
		.wBitsPerSample = sizeof(s16) * 8,
	};

	// DWORD latencySamples = waveFormat.nSamplesPerSec / 15u;
	IDirectSoundBuffer* buffer;
};

// TODO REF: try to get rid of globals
// TODO REF: attach methods to structs after day 020
static bool globalIsAppRunning = true;
static u64 globalPerfFrequency;
static Screen screen = {};
static Sound sound = {};

static void InitDirectSound(HWND window) {
	typedef HRESULT WINAPI DirectSoundCreate(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
	HMODULE directSoundLibrary = LoadLibraryA("dsound.dll");
	if (!directSoundLibrary)
		return;

	#pragma warning(suppress: 4191)
	auto directSoundCreate = (DirectSoundCreate*)GetProcAddress(directSoundLibrary, "DirectSoundCreate");
	if (!directSoundCreate)
		return;

	IDirectSound* directSound;
	if (!SUCCEEDED(directSoundCreate(0, &directSound, 0)))
		return;

	if (!SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
		return;

	DSBUFFERDESC primaryBufferDesc = {
		.dwSize = sizeof(primaryBufferDesc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};

	IDirectSoundBuffer* primaryBuffer;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&primaryBufferDesc, &primaryBuffer, 0)))
		return;

	if (!SUCCEEDED(primaryBuffer->SetFormat(&sound.waveFormat)))
		return;

	DSBUFFERDESC soundBufferDesc = {
		.dwSize = sizeof(soundBufferDesc),
		.dwBufferBytes = sound.waveFormat.nAvgBytesPerSec,
		.lpwfxFormat = &sound.waveFormat
	};
	directSound->CreateSoundBuffer(&soundBufferDesc, &sound.buffer, 0);
}

static void FillSoundBuffer(Game::SoundBuffer* source, DWORD lockCursor, DWORD bytesToWrite, u32* runningSampleIndex) {
	void* region1; DWORD region1Size; void* region2; DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(lockCursor, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0)))
		return;

	// TODO REF: remove block when runningSampleIndex will go away
	// -------------------------------------------------------
	u32 sampleSize = sizeof(*(source->samples));
	u32 region1SizeInSamples = region1Size / sampleSize;
	u32 region2SizeInSamples = region2Size / sampleSize;
	*runningSampleIndex += (region1SizeInSamples + region2SizeInSamples) / 2;
	// -------------------------------------------------------

	std::memcpy(region1, source->samples, region1Size);
	std::memcpy(region2, (std::byte*)source->samples + region1Size, region2Size);
	// TODO FEAT: what should we do if Unlock failed?
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ClearSoundBuffer() {
	void* region1; DWORD region1Size; void* region2; DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(0, sound.waveFormat.nAvgBytesPerSec, &region1, &region1Size, &region2, &region2Size, 0)))
		return;

	std::memset(region1, 0, region1Size);
	std::memset(region2, 0, region2Size);
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ResizeScreenBuffer(u32 width, u32 height) {
	if (screen.memory)
		VirtualFree(screen.memory, 0, MEM_RELEASE);
		
	screen.setWidth(width);
	screen.setHeight(height);
	screen.memory = VirtualAlloc(0, screen.getMemorySize(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void DisplayScreenBuffer(HWND window, HDC deviceContext) {
	RECT clientRect;
	GetClientRect(window, &clientRect);

	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;

	StretchDIBits(deviceContext,
		0, 0, clientWidth, clientHeight,
		0, 0, (int)screen.getWidth(), (int)screen.getHeight(),
		screen.memory, &screen.bitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

static LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);
			DisplayScreenBuffer(window, deviceContext);
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

		default: {
			return DefWindowProcA(window, message, wParam, lParam);
		};
	}

	return 0;
}

static void ProcessPendingMessages(Game::Input* gameInput) {
	gameInput->ResetTransitionsCount();

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
				bool wasKeyPressed = message.lParam & (1u << 30);
				bool isKeyPressed = !(message.lParam & (1u << 31));

				if (wasKeyPressed == isKeyPressed)
					continue;

				Game::ButtonState* buttonState;

				switch (message.wParam) {
					case VK_RETURN: {
						buttonState = &gameInput->start;
					} break;
					case VK_ESCAPE: {
						buttonState = &gameInput->back;
					} break;
					case VK_UP: {
						buttonState = &gameInput->moveUp;
					} break;
					case VK_DOWN: {
						buttonState = &gameInput->moveDown;
					} break;
					case VK_LEFT: {
						buttonState = &gameInput->moveLeft;
					} break;
					case VK_RIGHT: {
						buttonState = &gameInput->moveRight;
					} break;
					case 'W': {
						buttonState = &gameInput->actionUp;
					} break;
					case 'S': {
						buttonState = &gameInput->actionDown;
					} break;
					case 'A': {
						buttonState = &gameInput->actionLeft;
					} break;
					case 'D': {
						buttonState = &gameInput->actionRight;
					} break;
					case 'Q': {
						buttonState = &gameInput->leftShoulder;
					} break;
					case 'E': {
						buttonState = &gameInput->rightShoulder;
					} break;
					default: {
						continue;
					}
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
	LARGE_INTEGER perfCounterResult;
	QueryPerformanceCounter(&perfCounterResult);
	return (u64)perfCounterResult.QuadPart;
}

static inline f32 GetSecondsElapsed(u64 start) {
	return (f32)(GetWallClock() - start) / (f32)globalPerfFrequency;
}

// TODO REF: add header file and reorder functions top-down
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
	static_assert(HANDMADE_DEV || !HANDMADE_SLOW);

	const u32 windowWidth = 1280;
	const u32 windowHeight = 720;

	u32 targetUpdateFrequency = 30;
	f32 targetSecondsPerFrame = 1.0f / (f32)targetUpdateFrequency;

	LARGE_INTEGER perfFrequencyResult;
	QueryPerformanceFrequency(&perfFrequencyResult);
	globalPerfFrequency = (u64)perfFrequencyResult.QuadPart;

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
		CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
		0, 0, hInstance, 0
	);

	HDC deviceContext = GetDC(window);
	ResizeScreenBuffer(windowWidth, windowHeight);

	InitDirectSound(window);

	if (sound.buffer) {
		ClearSoundBuffer();
		
		// TODO FEAT: address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
		// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
		// try to switch sound to XAudio2 after day 025, and check if address sanitizer problem goes away
		sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	s16* soundSamples = (s16*)VirtualAlloc(0, sound.waveFormat.nAvgBytesPerSec, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	void* gameMemoryBaseAddress = 0;
	if constexpr (HANDMADE_DEV && INTPTR_MAX == INT64_MAX) {
		gameMemoryBaseAddress = (void*)1024_GB;
	}

	size_t gameMemorySize = 1_GB;
	void* gameMemoryStorage = VirtualAlloc(gameMemoryBaseAddress, gameMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!gameMemoryStorage)
		return 0;

	Game::Memory gameMemory = {
		.permanentStorageSize = 64_MB,
		.transientStorageSize = gameMemorySize - gameMemory.permanentStorageSize,
		.permanentStorage = gameMemoryStorage,
		.transientStorage = (std::byte*)gameMemory.permanentStorage + gameMemory.permanentStorageSize
	};

	Game::Input gameInput = {};

	u64 startWallClock = GetWallClock();
	u64 startCycleCounter = __rdtsc();

	u32 runningSampleIndex = 0;

	while (globalIsAppRunning) {
		ProcessPendingMessages(&gameInput);

		Game::ScreenBuffer gameScreenBuffer = {
			.width = screen.getWidth(),
			.height = screen.getHeight(),
			.memory = (u32*)screen.memory,
		};

		// TODO BUG: sound changes frequency periodically
		// TODO FEAT: make sure that game not crashes if sound was not initialized
		DWORD lockCursor = 0;
		DWORD bytesToWrite = 0;

		DWORD playCursor; DWORD writeCursor;
		if (sound.buffer && SUCCEEDED(sound.buffer->GetCurrentPosition(&playCursor, &writeCursor))) {
			// TODO BUG: we have sound pitches because we rewriting memory under playCursor when we use soundLatencySamples
			// DWORD targetCursor = (playCursor + sound.latencySamples * sound.waveFormat.nBlockAlign) % sound.waveFormat.nAvgBytesPerSec;
			DWORD targetCursor = playCursor % sound.waveFormat.nAvgBytesPerSec;
			lockCursor = (runningSampleIndex * sound.waveFormat.nBlockAlign) % sound.waveFormat.nAvgBytesPerSec;
			bytesToWrite = lockCursor > targetCursor ? sound.waveFormat.nAvgBytesPerSec : 0;
			bytesToWrite += targetCursor - lockCursor;
		}

		Game::SoundBuffer gameSoundBuffer = {
			.samplesPerSecond = sound.waveFormat.nSamplesPerSec,
			.samplesToWrite = bytesToWrite / sound.waveFormat.nBlockAlign,
			.samples = soundSamples,
		};

		Game::UpdateAndRender(&gameInput, &gameMemory, &gameScreenBuffer, &gameSoundBuffer);
		FillSoundBuffer(&gameSoundBuffer, lockCursor, bytesToWrite, &runningSampleIndex);

		f32 frameSecondsElapsed = GetSecondsElapsed(startWallClock);
		if (sleepIsGranular && frameSecondsElapsed < targetSecondsPerFrame) {
			DWORD sleepErrorMs = 2;
			DWORD sleepMs = (DWORD)(1000.0f * (targetSecondsPerFrame - frameSecondsElapsed));
			if (sleepMs > sleepErrorMs) Sleep(sleepMs - sleepErrorMs);
		}

		frameSecondsElapsed = GetSecondsElapsed(startWallClock);
		// TODO: change assert to log, because sometimes sleep is late anyway
		if (globalIsAppRunning) assert(frameSecondsElapsed < targetSecondsPerFrame);

		while (frameSecondsElapsed < targetSecondsPerFrame)
			frameSecondsElapsed = GetSecondsElapsed(startWallClock);
		
		DisplayScreenBuffer(window, deviceContext);

		u64 endCycleCounter = __rdtsc();
		u64 cycleCounterElapsed = endCycleCounter - startCycleCounter;
		u32 framesPerSecond = (u32)(1 / frameSecondsElapsed);
		u32 millisecondsPerFrame = (u32)(1000 * frameSecondsElapsed);
		u32 megaCyclesPerFrame = (u32)(cycleCounterElapsed / (1000 * 1000));
		char outputBuffer[256];
		wsprintfA(outputBuffer, "%d f/s, %d ms/f, %d Mc/f\n", framesPerSecond, millisecondsPerFrame, megaCyclesPerFrame);
		OutputDebugStringA(outputBuffer);
		
		startWallClock = GetWallClock();
		startCycleCounter = __rdtsc();
	}

	return 0;
}

namespace Platform {
	static void FreeFileMemory(void* memory) {
		if (!memory)
			return;

		HANDLE heapHandle = GetProcessHeap();
		if (!heapHandle)
			return;

		HeapFree(heapHandle, 0, memory);
		memory = 0;
	}

	static ReadEntireFileResult ReadEntireFile(const char* fileName) {
		ReadEntireFileResult result = {};
		u32 memorySize = 0;
		void* memory = 0;

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
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0) && (bytesWritten == memorySize)) {
			result = true;
		}

		CloseHandle(fileHandle);
		return result;
	}
}