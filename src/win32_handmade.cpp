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

	u8 __padding[4];

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
		.nAvgBytesPerSec = sizeof(u16) * waveFormat.nChannels * waveFormat.nSamplesPerSec,
		.nBlockAlign = sizeof(u16) * waveFormat.nChannels,
		.wBitsPerSample = sizeof(u16) * 8,
	};

	u8 __padding[2];

	DWORD latencySamples = waveFormat.nSamplesPerSec / 15u;
	IDirectSoundBuffer* buffer;
};

static bool isAppRunning = true;

// TODO: move to winmain and attach related methods
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
	if (!SUCCEEDED(directSoundCreate(nullptr, &directSound, 0))) {
		return;
	}

	directSound->SetCooperativeLevel(window, DSSCL_PRIORITY);

	DSBUFFERDESC primaryBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};

	IDirectSoundBuffer* primaryBuffer;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&primaryBufferDesc, &primaryBuffer, 0))) {
		return;
	};

	if (!SUCCEEDED(primaryBuffer->SetFormat(&sound.waveFormat))) {
		return;
	}

	DSBUFFERDESC soundBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwBufferBytes = sound.waveFormat.nAvgBytesPerSec,
		.lpwfxFormat = &sound.waveFormat
	};

	if (!SUCCEEDED(directSound->CreateSoundBuffer(&soundBufferDesc, &sound.buffer, 0))) {
		return;
	};
}

static void FillSoundBuffer(Game::SoundBuffer* source, DWORD lockCursor, DWORD bytesToWrite, u32* runningSampleIndex) {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(lockCursor, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
		return;
	}

	s16* sourceSamples = source->samples;

	u32 region1SizeSamples = region1Size / sound.waveFormat.nBlockAlign;
	s16* destSamples = static_cast<s16*>(region1);

	for (u32 i = 0; i < region1SizeSamples; i++) {
		*destSamples++ = *sourceSamples++;
		*destSamples++ = *sourceSamples++;
		(*runningSampleIndex)++;
	}

	u32 region2SizeSamples = region2Size / sound.waveFormat.nBlockAlign;
	destSamples = static_cast<s16*>(region2);

	for (u32 i = 0; i < region2SizeSamples; i++) {
		*destSamples++ = *sourceSamples++;
		*destSamples++ = *sourceSamples++;
		(*runningSampleIndex)++;
	}

	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ClearSoundBuffer() {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(0, sound.waveFormat.nAvgBytesPerSec, &region1, &region1Size, &region2, &region2Size, 0))) {
		return;
	}

	u8* regionToClean = static_cast<u8*>(region1);
	for (u32 i = 0; i < region1Size; i++) {
		*regionToClean++ = 0;
	}

	regionToClean = static_cast<u8*>(region2);
	for (u32 i = 0; i < region2Size; i++) {
		*regionToClean++ = 0;
	}

	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ResizeScreenBuffer(u16 width, u16 height) {
	if (screen.memory) {
		VirtualFree(screen.memory, 0, MEM_RELEASE);
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
		0, windowClass.lpszClassName, "Handmade Hero", WS_TILEDWINDOW | WS_VISIBLE,
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
	sound.buffer->Play(0, 0, DSBPLAY_LOOPING);

	s16* soundSamples =  static_cast<s16*>(
		VirtualAlloc(nullptr, sound.waveFormat.nAvgBytesPerSec, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
	);

	Game::Memory gameMemory = {
		.permanentStorageSize = Megabytes(64),
		.transientStorageSize = Megabytes(4096),
		// TODO: use single VirtualAlloc for both storages
		// TODO: set baseAddress based on compilation flag
		.permanentStorage = VirtualAlloc(nullptr, gameMemory.permanentStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE),
		.transientStorage = VirtualAlloc(nullptr, gameMemory.transientStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
	};

	if (!gameMemory.permanentStorage || !gameMemory.transientStorage) {
		return 0;
	}

	// LARGE_INTEGER perfFrequency;
	// QueryPerformanceFrequency(&perfFrequency);
	// LARGE_INTEGER startPerfCounter;
	// QueryPerformanceCounter(&startPerfCounter);
	// u64 startCycleCounter = __rdtsc();

	u32 runningSampleIndex = 0;

	while (isAppRunning) {
		MSG message;
		BOOL peekMessageResult = PeekMessageA(&message, window, 0, 0, PM_REMOVE);

		while (peekMessageResult) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
			peekMessageResult = PeekMessageA(&message, window, 0, 0, PM_REMOVE);
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