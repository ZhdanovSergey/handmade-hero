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

	PADDING_4

	void* memory;

	void setWidth(u16 width) {
		bitmapInfo.bmiHeader.biWidth = width;
	}

	u16 getWidth() {
		return static_cast<u16>(bitmapInfo.bmiHeader.biWidth);
	}

	void setHeight(u16 height) {
		// biHeight is negative in order to top-left pixel been first in bitmap
		bitmapInfo.bmiHeader.biHeight = - height;
	}

	u16 getHeight() {
		long positiveHeight = std::abs(bitmapInfo.bmiHeader.biHeight);
		return static_cast<u16>(positiveHeight);
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

	PADDING_2
	
	PADDING_4
	// DWORD latencySamples = waveFormat.nSamplesPerSec / 15u;

	IDirectSoundBuffer* buffer;
};

// TODO REF: try to get rid of globals
// TODO REF: attach methods to structs after day 020
static bool isAppRunning = true;
static Screen screen = {};
static Sound sound = {};

static void InitDirectSound(HWND window) {
	typedef HRESULT WINAPI DirectSoundCreate(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
	HMODULE directSoundLibrary = LoadLibraryA("dsound.dll");
	if (!directSoundLibrary)
		return;

	#pragma warning(suppress: 4191)
	auto directSoundCreate = reinterpret_cast<DirectSoundCreate*>(GetProcAddress(directSoundLibrary, "DirectSoundCreate"));
	if (!directSoundCreate)
		return;

	IDirectSound* directSound;
	if (!SUCCEEDED(directSoundCreate(nullptr, &directSound, nullptr)))
		return;

	if (!SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
		return;

	DSBUFFERDESC primaryBufferDesc = {
		.dwSize = sizeof(primaryBufferDesc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};

	IDirectSoundBuffer* primaryBuffer;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&primaryBufferDesc, &primaryBuffer, nullptr)))
		return;

	if (!SUCCEEDED(primaryBuffer->SetFormat(&sound.waveFormat)))
		return;

	DSBUFFERDESC soundBufferDesc = {
		.dwSize = sizeof(soundBufferDesc),
		.dwBufferBytes = sound.waveFormat.nAvgBytesPerSec,
		.lpwfxFormat = &sound.waveFormat
	};
	directSound->CreateSoundBuffer(&soundBufferDesc, &sound.buffer, nullptr);
}

static void FillSoundBuffer(Game::SoundBuffer* source, DWORD lockCursor, DWORD bytesToWrite, u32* runningSampleIndex) {
	void* region1; DWORD region1Size; void* region2; DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(lockCursor, bytesToWrite, &region1, &region1Size, &region2, &region2Size, NULL)))
		return;

	// TODO REF: remove block when runningSampleIndex will go away
	// -------------------------------------------------------
	constexpr u32 sampleSize = sizeof(*(source->samples));
	u32 region1SizeInSamples = region1Size / sampleSize;
	u32 region2SizeInSamples = region2Size / sampleSize;
	*runningSampleIndex += (region1SizeInSamples + region2SizeInSamples) / 2;
	// -------------------------------------------------------

	std::memcpy(region1, source->samples, region1Size);
	std::memcpy(region2, reinterpret_cast<std::byte*>(source->samples) + region1Size, region2Size);
	// TODO FEAT: what should we do if Unlock failed?
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ClearSoundBuffer() {
	void* region1; DWORD region1Size; void* region2; DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(0, sound.waveFormat.nAvgBytesPerSec, &region1, &region1Size, &region2, &region2Size, NULL)))
		return;

	std::memset(region1, 0, region1Size);
	std::memset(region2, 0, region2Size);
	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ResizeScreenBuffer(u16 width, u16 height) {
	if (screen.memory)
		VirtualFree(screen.memory, NULL, MEM_RELEASE);
		
	screen.setWidth(width);
	screen.setHeight(height);
	screen.memory = VirtualAlloc(nullptr, screen.getMemorySize(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void DisplayScreenBuffer(HWND window, HDC deviceContext) {
	RECT clientRect;
	GetClientRect(window, &clientRect);
	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;

	StretchDIBits(deviceContext,
		0, 0, clientWidth, clientHeight,
		0, 0, screen.getWidth(), screen.getHeight(),
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
			isAppRunning = false;
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

static void ProcessPendingMessages(Game::Input* pGameInput) {
	pGameInput->ResetTransitionsCount();

	MSG message;
	while (PeekMessageA(&message, NULL, NULL, NULL, PM_REMOVE)) {
		switch (message.message) {
			case WM_QUIT: {
				isAppRunning = false;
			} break;
			case WM_KEYUP:
			case WM_KEYDOWN:
			case WM_SYSKEYUP:
			case WM_SYSKEYDOWN: {
				bool wasKeyPressed = message.lParam & (1u << 30);
				bool isKeyPressed = !(message.lParam & (1u << 31));

				if (wasKeyPressed == isKeyPressed)
					continue;

				Game::ButtonState* pButtonState;

				switch (message.wParam) {
					case VK_RETURN: {
						pButtonState = &pGameInput->start;
					} break;
					case VK_ESCAPE: {
						pButtonState = &pGameInput->back;
					} break;
					case VK_UP: {
						pButtonState = &pGameInput->moveUp;
					} break;
					case VK_DOWN: {
						pButtonState = &pGameInput->moveDown;
					} break;
					case VK_LEFT: {
						pButtonState = &pGameInput->moveLeft;
					} break;
					case VK_RIGHT: {
						pButtonState = &pGameInput->moveRight;
					} break;
					case 'W': {
						pButtonState = &pGameInput->actionUp;
					} break;
					case 'S': {
						pButtonState = &pGameInput->actionDown;
					} break;
					case 'A': {
						pButtonState = &pGameInput->actionLeft;
					} break;
					case 'D': {
						pButtonState = &pGameInput->actionRight;
					} break;
					case 'Q': {
						pButtonState = &pGameInput->leftShoulder;
					} break;
					case 'E': {
						pButtonState = &pGameInput->rightShoulder;
					} break;
					default: {
						continue;
					}
				}

				pButtonState->isEndedPressed = isKeyPressed;
				pButtonState->transitionsCount++;
			} break;
			default: {
				TranslateMessage(&message);
				DispatchMessageA(&message);
			}
		}
	}
}

// TODO REF: add header file and reorder functions top-down
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
	static_assert(HANDMADE_DEV || !HANDMADE_SLOW);

	constexpr u16 windowWidth = 1280;
	constexpr u16 windowHeight = 720;

	WNDCLASSA windowClass = {
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = MainWindowCallback,
		.hInstance = hInstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	RegisterClassA(&windowClass);

	HWND window = CreateWindowExA(
		NULL, windowClass.lpszClassName, "Handmade Hero", WS_TILEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
		nullptr, nullptr, hInstance, nullptr
	);

	HDC deviceContext = GetDC(window);
	ResizeScreenBuffer(windowWidth, windowHeight);

	InitDirectSound(window);

	if (sound.buffer) {
		ClearSoundBuffer();
		
		// TODO FEAT: address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
		// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
		// try to switch sound to XAudio2 after day 025, and check if address sanitizer problem goes away
		sound.buffer->Play(NULL, NULL, DSBPLAY_LOOPING);
	}

	s16* soundSamples =  static_cast<s16*>(
		VirtualAlloc(nullptr, sound.waveFormat.nAvgBytesPerSec, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
	);

	void* gameMemoryBaseAddress = nullptr;
	if constexpr (HANDMADE_DEV) {
		gameMemoryBaseAddress = reinterpret_cast<void*>(1024_GB);
	}

	constexpr size_t gameMemorySize = 1_GB;
	void* gameMemoryStorage = VirtualAlloc(gameMemoryBaseAddress, gameMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!gameMemoryStorage)
		return 0;

	Game::Memory gameMemory = {
		.permanentStorageSize = 64_MB,
		.transientStorageSize = gameMemorySize - gameMemory.permanentStorageSize,
		.permanentStorage = gameMemoryStorage,
		.transientStorage = static_cast<std::byte*>(gameMemory.permanentStorage) + gameMemory.permanentStorageSize
	};

	Game::Input gameInput = {};

	// LARGE_INTEGER perfFrequency;
	// QueryPerformanceFrequency(&perfFrequency);
	// LARGE_INTEGER startPerfCounter;
	// QueryPerformanceCounter(&startPerfCounter);
	// u64 startCycleCounter = __rdtsc();

	u32 runningSampleIndex = 0;

	while (isAppRunning) {
		ProcessPendingMessages(&gameInput);

		Game::ScreenBuffer gameScreenBuffer = {
			.width = screen.getWidth(),
			.height = screen.getHeight(),
			.memory = static_cast<u32*>(screen.memory),
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
		DisplayScreenBuffer(window, deviceContext);
		FillSoundBuffer(&gameSoundBuffer, lockCursor, bytesToWrite, &runningSampleIndex);

		// u64 endCycleCounter = __rdtsc();
		// u64 cycleCounterElapsed = endCycleCounter - startCycleCounter;
		// LARGE_INTEGER endPerfCounter;
		// QueryPerformanceCounter(&endPerfCounter);
		// s64 perfCounterElapsed = endPerfCounter.QuadPart - startPerfCounter.QuadPart;
		// s32 framesPerSecond = static_cast<s32>(perfFrequency.QuadPart / perfCounterElapsed);
		// s32 millisecondsPerFrame = static_cast<s32>(1000 * perfCounterElapsed / perfFrequency.QuadPart);
		// s32 megaCyclesPerFrame = static_cast<s32>(cycleCounterElapsed / (1000 * 1000));
		// char outputBuffer[256];
		// wsprintfA(outputBuffer, "%d f/s, %d ms/f, %d Mc/f\n", framesPerSecond, millisecondsPerFrame, megaCyclesPerFrame);
		// OutputDebugStringA(outputBuffer);
		// startPerfCounter = endPerfCounter;
		// startCycleCounter = endCycleCounter;
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

		HeapFree(heapHandle, NULL, memory);
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

		HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, NULL, nullptr);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return result;

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(fileHandle, &fileSize))
			goto close_file_handle;

		memorySize = SafeTruncateToU32(fileSize.QuadPart);
		memory = HeapAlloc(heapHandle, NULL, memorySize);
		if (!memory)
			goto close_file_handle;

		DWORD bytesRead;
		if (!ReadFile(fileHandle, memory, memorySize, &bytesRead, nullptr) || (bytesRead != memorySize)) {
			HeapFree(heapHandle, NULL, memory);
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

		HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, NULL, nullptr, CREATE_ALWAYS, NULL, nullptr);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return result;

		DWORD bytesWritten;
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, nullptr) && (bytesWritten == memorySize)) {
			result = true;
		}

		CloseHandle(fileHandle);
		return result;
	}
}