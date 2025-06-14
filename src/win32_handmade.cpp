#include "globals.h"
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

	std::byte __padding[4];

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
		long positiveHeight = abs(bitmapInfo.bmiHeader.biHeight);
		return static_cast<u16>(positiveHeight);
	}

	u64 getMemorySize() {
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

	std::byte __padding[2];

	DWORD latencySamples = waveFormat.nSamplesPerSec / 15u;
	IDirectSoundBuffer* buffer;
};

// TODO: try to get rid of globals
static bool isAppRunning = true;
// TODO: attach methods to structs
static Screen screen = {};
static Sound sound = {};

static void InitDirectSound(HWND window) {
	typedef HRESULT DirectSoundCreate(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
	HMODULE directSoundLibrary = LoadLibraryA("dsound.dll");

	if (!directSoundLibrary) {
		return;
	}

	#pragma warning(suppress: 4191)
	DirectSoundCreate* directSoundCreate = reinterpret_cast<DirectSoundCreate*>
		(GetProcAddress(directSoundLibrary, "DirectSoundCreate"));

	if (!directSoundCreate) {
		return;
	}

	IDirectSound* directSound;
	if (!SUCCEEDED(directSoundCreate(nullptr, &directSound, nullptr))) {
		return;
	}

	directSound->SetCooperativeLevel(window, DSSCL_PRIORITY);

	DSBUFFERDESC primaryBufferDesc = {
		.dwSize = sizeof(primaryBufferDesc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};

	IDirectSoundBuffer* primaryBuffer;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&primaryBufferDesc, &primaryBuffer, nullptr))) {
		return;
	};

	if (!SUCCEEDED(primaryBuffer->SetFormat(&sound.waveFormat))) {
		return;
	}

	DSBUFFERDESC soundBufferDesc = {
		.dwSize = sizeof(soundBufferDesc),
		.dwBufferBytes = sound.waveFormat.nAvgBytesPerSec,
		.lpwfxFormat = &sound.waveFormat
	};

	if (!SUCCEEDED(directSound->CreateSoundBuffer(&soundBufferDesc, &sound.buffer, nullptr))) {
		return;
	};
}

static void FillSoundBuffer(Game::SoundBuffer* source, DWORD lockCursor, DWORD bytesToWrite, u32* runningSampleIndex) {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(lockCursor, bytesToWrite, &region1, &region1Size, &region2, &region2Size, NULL))) {
		return;
	}

	// TODO: remove block when runningSampleIndex will go away
	// -------------------------------------------------------
	const u32 sampleSize = sizeof(*(source->samples));
	u32 region1SizeInSamples = region1Size / sampleSize;
	u32 region2SizeInSamples = region2Size / sampleSize;
	*runningSampleIndex += (region1SizeInSamples + region2SizeInSamples) / 2;
	// -------------------------------------------------------

	std::memcpy(region1, source->samples, region1Size);
	std::memcpy(region2, reinterpret_cast<std::byte*>(source->samples) + region1Size, region2Size);

	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ClearSoundBuffer() {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(0, sound.waveFormat.nAvgBytesPerSec, &region1, &region1Size, &region2, &region2Size, NULL))) {
		return;
	}

	std::memset(region1, 0, region1Size);
	std::memset(region2, 0, region2Size);

	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ResizeScreenBuffer(u16 width, u16 height) {
	if (screen.memory) {
		VirtualFree(screen.memory, NULL, MEM_RELEASE);
	}

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

static LRESULT MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);
			DisplayScreenBuffer(window, deviceContext);
			EndPaint(window, &paint);
		} break;

		case WM_KEYUP:
		case WM_KEYDOWN: {
			bool isKeyDown = !(lParam & (1u << 31));
			bool wasKeyDown = lParam & (1u << 30);

			if (!isKeyDown || wasKeyDown) {
				return 0;
			}

			switch (wParam) {
				case VK_UP: {
					OutputDebugStringA("VK_UP\n");
				} break;

				case VK_DOWN: {
					OutputDebugStringA("VK_DOWN\n");
				} break;

				case VK_LEFT: {
					OutputDebugStringA("VK_LEFT\n");
				} break;

				case VK_RIGHT: {
					OutputDebugStringA("VK_RIGHT\n");
				} break;
			}

		} break;

		case WM_CLOSE:
		case WM_DESTROY: {
			isAppRunning = false;
		} break;

		default: {
			return DefWindowProcA(window, message, wParam, lParam);
		};
	}

	return 0;
}

int wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
	static_assert(HANDMADE_INTERNAL || !HANDMADE_SLOW);

	const u16 windowWidth = 1280;
	const u16 windowHeight = 720;

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
	ClearSoundBuffer();

	// TODO: address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
	// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
	// try to switch sound to XAudio2 after day 025, and check if address sanitizer problem goes away
	sound.buffer->Play(NULL, NULL, DSBPLAY_LOOPING);

	s16* soundSamples =  static_cast<s16*>(
		VirtualAlloc(nullptr, sound.waveFormat.nAvgBytesPerSec, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
	);

	void* gameMemoryBaseAddress = nullptr;

	if constexpr (HANDMADE_INTERNAL) {
		gameMemoryBaseAddress = reinterpret_cast<void*>(1024_GB);
	}

	const u64 gameMemorySize = 4_GB;
	void* gameMemoryStorage = VirtualAlloc(gameMemoryBaseAddress, gameMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (!gameMemoryStorage) {
		return 0;
	}

	Game::Memory gameMemory = {
		.permanentStorageSize = 64_MB,
		.transientStorageSize = gameMemorySize - gameMemory.permanentStorageSize,
		.permanentStorage = gameMemoryStorage,
		.transientStorage = static_cast<std::byte*>(gameMemory.permanentStorage) + gameMemory.permanentStorageSize
	};

	// LARGE_INTEGER perfFrequency;
	// QueryPerformanceFrequency(&perfFrequency);
	// LARGE_INTEGER startPerfCounter;
	// QueryPerformanceCounter(&startPerfCounter);
	// u64 startCycleCounter = __rdtsc();

	u32 runningSampleIndex = 0;

	while (isAppRunning) {
		MSG message;
		while (PeekMessageA(&message, window, NULL, NULL, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}

		Game::ScreenBuffer gameScreenBuffer = {
			.width = screen.getWidth(),
			.height = screen.getHeight(),
			.memory = static_cast<u32*>(screen.memory),
		};

		DWORD playCursor;
		DWORD writeCursor;
		DWORD lockCursor = 0;
		DWORD bytesToWrite = 0;
		if (sound.buffer && SUCCEEDED(sound.buffer->GetCurrentPosition(&playCursor, &writeCursor))) {
			// NOTE: it seems like we rewriting memory under playCursor because of soundLatencySamples right now
			DWORD targetCursor = (playCursor + sound.latencySamples * sound.waveFormat.nBlockAlign) % sound.waveFormat.nAvgBytesPerSec;
			lockCursor = (runningSampleIndex * sound.waveFormat.nBlockAlign) % sound.waveFormat.nAvgBytesPerSec;
			bytesToWrite = lockCursor > targetCursor ? sound.waveFormat.nAvgBytesPerSec : 0;
			bytesToWrite += targetCursor - lockCursor;
		}

		Game::SoundBuffer gameSoundBuffer = {
			.samplesPerSecond = sound.waveFormat.nSamplesPerSec,
			.samplesToWrite = bytesToWrite / sound.waveFormat.nBlockAlign,
			.samples = soundSamples,
		};

		Game::UpdateAndRender(&gameMemory, &gameScreenBuffer, &gameSoundBuffer);
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
		if (!memory) return;

		HANDLE heapHandle = GetProcessHeap();
		if (!heapHandle) return;

		HeapFree(heapHandle, NULL, memory);
		memory = nullptr;
	}

	static ReadEntireFileResult ReadEntireFile(const char* fileName) {
		ReadEntireFileResult result = {};
		u32 memorySize = 0;
		void* memory = nullptr;

		// seems like this handle don't need to be closed
		HANDLE heapHandle = GetProcessHeap();
		if (!heapHandle) return result;

		HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, NULL, nullptr);
		if (fileHandle == INVALID_HANDLE_VALUE) return result;

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(fileHandle, &fileSize)) goto close_file_handle;

		memorySize = SafeTruncateToU32(fileSize.QuadPart);
		memory = HeapAlloc(heapHandle, NULL, memorySize);
		if (!memory) goto close_file_handle;

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
		if (fileHandle == INVALID_HANDLE_VALUE) return result;

		DWORD bytesWritten;
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, nullptr) && (bytesWritten == memorySize)) {
			result = true;
		}

		CloseHandle(fileHandle);
		return result;
	}
}