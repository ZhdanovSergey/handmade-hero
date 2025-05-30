#include "globals.h"
#include "game.cpp"

#include <windows.h>
#include <dsound.h>

struct Screen {
	BITMAPINFO bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	};

	private: uint8 __padding[4]; public:

	void* memory; // uint32

	void setWidth(uint32 width) {
		bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(width);
	}

	uint32 getWidth() {
		return static_cast<uint32>(bitmapInfo.bmiHeader.biWidth);
	}

	void setHeight(uint32 height) {
		// biHeight is negative in order to top-left pixel been first in bitmap
		bitmapInfo.bmiHeader.biHeight = - static_cast<LONG>(height);
	}

	uint32 getHeight() {
		LONG positiveHeight = abs(bitmapInfo.bmiHeader.biHeight);
		return static_cast<uint32>(positiveHeight);
	}

	uint32 getMemorySize() {
		uint32 bytesPerPixel = bitmapInfo.bmiHeader.biBitCount / 8u;
		return bytesPerPixel * getWidth() * getHeight();
	}
};

struct Sound {
	IDirectSoundBuffer* buffer = nullptr;
	
	WAVEFORMATEX waveFormat = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = 48000,
		.nAvgBytesPerSec = sizeof(uint16) * waveFormat.nChannels * waveFormat.nSamplesPerSec,
		.nBlockAlign = sizeof(uint16) * waveFormat.nChannels,
		.wBitsPerSample = sizeof(uint16) * 8,
	};

	private: uint8 __padding[2]; public:

	DWORD latencySamples = waveFormat.nSamplesPerSec / 15u;
};

bool isAppRunning = true;

// TODO: move to winmain and attach related methods
Screen screen = {};
Sound sound = {};

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

static void FillSoundBuffer(Game::SoundBuffer* source, DWORD lockCursor, DWORD bytesToWrite, uint32* runningSampleIndex) {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(sound.buffer->Lock(lockCursor, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
		return;
	}

	int16* sourceSamples = source->samples;

	uint32 region1SizeSamples = region1Size / sound.waveFormat.nBlockAlign;
	int16* destSamples = static_cast<int16*>(region1);

	for (uint32 i = 0; i < region1SizeSamples; i++) {
		*destSamples++ = *sourceSamples++;
		*destSamples++ = *sourceSamples++;
		(*runningSampleIndex)++;
	}

	uint32 region2SizeSamples = region2Size / sound.waveFormat.nBlockAlign;
	destSamples = static_cast<int16*>(region2);

	for (uint32 i = 0; i < region2SizeSamples; i++) {
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

	uint8* regionToClean = static_cast<uint8*>(region1);
	for (uint32 i = 0; i < region1Size; i++) {
		*regionToClean++ = 0;
	}

	regionToClean = static_cast<uint8*>(region2);
	for (uint32 i = 0; i < region2Size; i++) {
		*regionToClean++ = 0;
	}

	sound.buffer->Unlock(region1, region1Size, region2, region2Size);
}

static void ResizeScreenBuffer(uint32 width, uint32 height) {
	if (screen.memory) {
		VirtualFree(screen.memory, 0, MEM_RELEASE);
	}

	screen.setWidth(width);
	screen.setHeight(height);
	
	SIZE_T memorySize = screen.getMemorySize();
	screen.memory = VirtualAlloc(nullptr, memorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void DisplayScreenBuffer(HWND window, HDC deviceContext) {
	RECT clientRect;
	GetClientRect(window, &clientRect);

	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;
	int screenWidth = static_cast<int>(screen.getWidth());
	int screenHeight = static_cast<int>(screen.getHeight());

	StretchDIBits(deviceContext,
		0, 0, clientWidth, clientHeight,
		0, 0, screenWidth, screenHeight,
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
	const uint32 windowWidth = 1280;
	const uint32 windowHeight = 720;

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
	sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
	// TODO: address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
	// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
	// try to switch sound to XAudio2 after day 025, and check if address sanitizer problem goes away

	int16* soundSamples =  static_cast<int16*>(
		VirtualAlloc(nullptr, sound.waveFormat.nAvgBytesPerSec, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
	);

	LARGE_INTEGER perfFrequency;
	QueryPerformanceFrequency(&perfFrequency);
	LARGE_INTEGER startPerfCounter;
	QueryPerformanceCounter(&startPerfCounter);
	uint64 startCycleCounter = __rdtsc();

	uint32 runningSampleIndex = 0;

	while (isAppRunning) {
		MSG message;
		BOOL peekMessageResult = PeekMessageA(&message, window, 0, 0, PM_REMOVE);

		while (peekMessageResult) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
			peekMessageResult = PeekMessageA(&message, window, 0, 0, PM_REMOVE);
		}

		Game::ScreenBuffer gameScreenBuffer = {
			.memory = static_cast<uint32*>(screen.memory),
			.width = screen.getWidth(),
			.height = screen.getHeight()
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
			.samples = soundSamples,
			.samplesPerSecond = sound.waveFormat.nSamplesPerSec,
			.samplesCount = bytesToWrite / sound.waveFormat.nBlockAlign,
		};

		Game::UpdateAndRender(&gameScreenBuffer, &gameSoundBuffer);
		DisplayScreenBuffer(window, deviceContext);
		FillSoundBuffer(&gameSoundBuffer, lockCursor, bytesToWrite, &runningSampleIndex);

		uint64 endCycleCounter = __rdtsc();
		uint64 cycleCounterElapsed = endCycleCounter - startCycleCounter;
		LARGE_INTEGER endPerfCounter;
		QueryPerformanceCounter(&endPerfCounter);
		int64 perfCounterElapsed = endPerfCounter.QuadPart - startPerfCounter.QuadPart;
		int32 framesPerSecond = static_cast<int32>(perfFrequency.QuadPart / perfCounterElapsed);
		int32 millisecondsPerFrame = static_cast<int32>(1000 * perfCounterElapsed / perfFrequency.QuadPart);
		int32 megaCyclesPerFrame = static_cast<int32>(cycleCounterElapsed / (1000 * 1000));
		char outputBuffer[256];
		wsprintfA(outputBuffer, "%d f/s, %d ms/f, %d Mc/f\n", framesPerSecond, millisecondsPerFrame, megaCyclesPerFrame);
		OutputDebugStringA(outputBuffer);
		startPerfCounter = endPerfCounter;
		startCycleCounter = endCycleCounter;
	}

	return 0;
}