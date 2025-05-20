#define _ALLOW_RTCc_IN_STL

#include <stdint.h>
#include <windows.h>
#include <dsound.h>
#include <math.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

const real32 PI = 3.14159265359f; // math.h will go soon

struct ScreenBuffer {
	BITMAPINFO bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	};

	private: char __padding[4]; public:

	void* memory = nullptr;

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

WAVEFORMATEX soundWaveFormat = {
	.wFormatTag = WAVE_FORMAT_PCM,
	.nChannels = 2,
	.nSamplesPerSec = 48000,
	.nAvgBytesPerSec = sizeof(uint16) * soundWaveFormat.nChannels * soundWaveFormat.nSamplesPerSec,
	.nBlockAlign = sizeof(uint16) * soundWaveFormat.nChannels,
	.wBitsPerSample = sizeof(uint16) * 8,
};

bool isAppRunning = true;
ScreenBuffer screenBuffer = {};
IDirectSoundBuffer* soundBuffer = nullptr;

void InitDirectSound(HWND window) {
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

	if (!SUCCEEDED(primaryBuffer->SetFormat(&soundWaveFormat))) {
		return;
	}

	DSBUFFERDESC soundBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwBufferBytes = soundWaveFormat.nAvgBytesPerSec,
		.lpwfxFormat = &soundWaveFormat
	};

	if (!SUCCEEDED(directSound->CreateSoundBuffer(&soundBufferDesc, &soundBuffer, 0))) {
		return;
	};
}

uint32 FillSoundSubRegion(void* region, DWORD regionSize, uint32 runningSampleIndex) {
	const uint32 frequency = 261;
	const uint32 volume = 5000;

	uint32 wavePeriodSamples = soundWaveFormat.nSamplesPerSec / frequency;
	uint32 regionSizeSamples = regionSize / soundWaveFormat.nBlockAlign;

	int16* sampleOut = static_cast<int16*>(region);

	for (uint32 sampleIndex = 0; sampleIndex < regionSizeSamples; sampleIndex++) {
		real32 sineValue = 2.0f * PI * static_cast<real32>(runningSampleIndex) / static_cast<real32>(wavePeriodSamples);
		int16 sampleValue = static_cast<int16>(sinf(sineValue) * volume);
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		runningSampleIndex++;
	}

	return runningSampleIndex;
}

uint32 FillSoundBuffer(DWORD byteToLock, DWORD bytesToWrite, uint32 runningSampleIndex) {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(soundBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
		return runningSampleIndex;
	}

	runningSampleIndex = FillSoundSubRegion(region1, region1Size, runningSampleIndex);
	runningSampleIndex = FillSoundSubRegion(region2, region2Size, runningSampleIndex);

	soundBuffer->Unlock(region1, region1Size, region2, region2Size);
	return runningSampleIndex;
}

void RenderGradient(uint32 blueOffset, uint32 greenOffset) {
	uint32* pixel = static_cast<uint32*>(screenBuffer.memory);

	for (uint32 y = 0; y < screenBuffer.getHeight(); y++) {
		for (uint32 x = 0; x < screenBuffer.getWidth(); x++) {
			uint32 green = (y + greenOffset) & UINT8_MAX;
			uint32 blue = (x + blueOffset) & UINT8_MAX;
			*pixel++ = (green << 8) | blue; // padding red green blue
		}
	}
}

void ResizeScreenBuffer(uint32 width, uint32 height) {
	if (screenBuffer.memory) {
		VirtualFree(screenBuffer.memory, 0, MEM_RELEASE);
	}

	screenBuffer.setWidth(width);
	screenBuffer.setHeight(height);
	
	SIZE_T memorySize = screenBuffer.getMemorySize();
	screenBuffer.memory = VirtualAlloc(nullptr, memorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void DisplayScreenBuffer(HWND window, HDC deviceContext) {
	RECT clientRect;
	GetClientRect(window, &clientRect);

	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;
	int screenWidth = static_cast<int>(screenBuffer.getWidth());
	int screenHeight = static_cast<int>(screenBuffer.getHeight());

	StretchDIBits(deviceContext,
		0, 0, clientWidth, clientHeight,
		0, 0, screenWidth, screenHeight,
		screenBuffer.memory, &screenBuffer.bitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

LRESULT MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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

	InitDirectSound(window);

	HDC deviceContext = GetDC(window);
	ResizeScreenBuffer(windowWidth, windowHeight);

	uint32 blueOffset = 0;
	uint32 runningSampleIndex = 0;

	runningSampleIndex = FillSoundBuffer(0, soundWaveFormat.nAvgBytesPerSec, runningSampleIndex);

	// address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
	// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
	// TODO: try to switch sound to XAudio2 after day 020, and check if address sanitizer problem go away
	soundBuffer->Play(0, 0, DSBPLAY_LOOPING);

	LARGE_INTEGER perfFrequency;
	QueryPerformanceFrequency(&perfFrequency);

	LARGE_INTEGER startPerfCounter;
	QueryPerformanceCounter(&startPerfCounter);

	uint64 startCycleCounter = __rdtsc();

	while (isAppRunning) {
		MSG message;
		BOOL peekMessageResult = PeekMessageA(&message, window, 0, 0, PM_REMOVE);

		while (peekMessageResult) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
			peekMessageResult = PeekMessageA(&message, window, 0, 0, PM_REMOVE);
		}
		
		RenderGradient(blueOffset, 0);
		DisplayScreenBuffer(window, deviceContext);
		blueOffset++;

		DWORD playCursor;
		DWORD writeCursor;
		if (!soundBuffer || !SUCCEEDED(soundBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
			continue;
		}

		DWORD byteToLock = (runningSampleIndex * soundWaveFormat.nBlockAlign) % soundWaveFormat.nAvgBytesPerSec;
		DWORD bytesToWrite = byteToLock > playCursor ? soundWaveFormat.nAvgBytesPerSec : 0;
		bytesToWrite += playCursor - byteToLock;

		runningSampleIndex = FillSoundBuffer(byteToLock, bytesToWrite, runningSampleIndex);

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