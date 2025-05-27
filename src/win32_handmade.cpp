#include "globals.h"
#include "main.cpp"

#include <windows.h>
#include <dsound.h>

struct Win32ScreenBuffer {
	BITMAPINFO bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	};

	private: char __padding[4]; public:

	void* memory;

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

// TODO: since we import everything here, I should probably get rid of global vars
bool isAppRunning = true;
Win32ScreenBuffer win32ScreenBuffer = {};

// TODO: wrap sound stuff in a struct
IDirectSoundBuffer* win32SoundBuffer = nullptr;
WAVEFORMATEX soundWaveFormat = {
	.wFormatTag = WAVE_FORMAT_PCM,
	.nChannels = 2,
	.nSamplesPerSec = 48000,
	.nAvgBytesPerSec = sizeof(uint16) * soundWaveFormat.nChannels * soundWaveFormat.nSamplesPerSec,
	.nBlockAlign = sizeof(uint16) * soundWaveFormat.nChannels,
	.wBitsPerSample = sizeof(uint16) * 8,
};
DWORD soundLatencySamples = soundWaveFormat.nSamplesPerSec / 15u;

static void Win32InitDirectSound(HWND window) {
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

	if (!SUCCEEDED(directSound->CreateSoundBuffer(&soundBufferDesc, &win32SoundBuffer, 0))) {
		return;
	};
}

static void Win32FillSoundBuffer(SoundBuffer* source, DWORD lockCursor, DWORD bytesToWrite, uint32* runningSampleIndex) {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(win32SoundBuffer->Lock(lockCursor, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
		return;
	}

	int16* sourceSamples = source->samples;

	uint32 region1SizeSamples = region1Size / soundWaveFormat.nBlockAlign;
	int16* destSamples = static_cast<int16*>(region1);

	for (uint32 i = 0; i < region1SizeSamples; i++) {
		*destSamples++ = *sourceSamples++;
		*destSamples++ = *sourceSamples++;
		(*runningSampleIndex)++;
	}

	uint32 region2SizeSamples = region2Size / soundWaveFormat.nBlockAlign;
	destSamples = static_cast<int16*>(region2);

	for (uint32 i = 0; i < region2SizeSamples; i++) {
		*destSamples++ = *sourceSamples++;
		*destSamples++ = *sourceSamples++;
		(*runningSampleIndex)++;
	}

	win32SoundBuffer->Unlock(region1, region1Size, region2, region2Size);
}

static void Win32ClearSoundBuffer() {
	void* region1;
	DWORD region1Size;
	void* region2;
	DWORD region2Size;
	if (!SUCCEEDED(win32SoundBuffer->Lock(0, soundWaveFormat.nAvgBytesPerSec, &region1, &region1Size, &region2, &region2Size, 0))) {
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

	win32SoundBuffer->Unlock(region1, region1Size, region2, region2Size);
}

static void Win32ResizeScreenBuffer(uint32 width, uint32 height) {
	if (win32ScreenBuffer.memory) {
		VirtualFree(win32ScreenBuffer.memory, 0, MEM_RELEASE);
	}

	win32ScreenBuffer.setWidth(width);
	win32ScreenBuffer.setHeight(height);
	
	SIZE_T memorySize = win32ScreenBuffer.getMemorySize();
	win32ScreenBuffer.memory = VirtualAlloc(nullptr, memorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void Win32DisplayScreenBuffer(HWND window, HDC deviceContext) {
	RECT clientRect;
	GetClientRect(window, &clientRect);

	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;
	int screenWidth = static_cast<int>(win32ScreenBuffer.getWidth());
	int screenHeight = static_cast<int>(win32ScreenBuffer.getHeight());

	StretchDIBits(deviceContext,
		0, 0, clientWidth, clientHeight,
		0, 0, screenWidth, screenHeight,
		win32ScreenBuffer.memory, &win32ScreenBuffer.bitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

static LRESULT Win32MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);
			Win32DisplayScreenBuffer(window, deviceContext);
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
		.lpfnWndProc = Win32MainWindowCallback,
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
	Win32ResizeScreenBuffer(windowWidth, windowHeight);
	Win32InitDirectSound(window);
	Win32ClearSoundBuffer();
	win32SoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
	// TODO: address sanitizer crashes program after Play() call, it seems to be known DirectSound problem
	// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
	// try to switch sound to XAudio2 after day 020, and check if address sanitizer problem go away

	int16* soundSamples =  static_cast<int16*>(
		VirtualAlloc(nullptr, soundWaveFormat.nAvgBytesPerSec, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
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

		ScreenBuffer screenBuffer = {
			.memory = win32ScreenBuffer.memory,
			.width = win32ScreenBuffer.getWidth(),
			.height = win32ScreenBuffer.getHeight()
		};

		DWORD playCursor;
		DWORD writeCursor;
		DWORD lockCursor = 0;
		DWORD bytesToWrite = 0;
		if (win32SoundBuffer && SUCCEEDED(win32SoundBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
			// TODO: it seems like we rewriting memory under playCursor because of soundLatencySamples right now
			DWORD targetCursor = (playCursor + soundLatencySamples * soundWaveFormat.nBlockAlign) % soundWaveFormat.nAvgBytesPerSec;
			lockCursor = (runningSampleIndex * soundWaveFormat.nBlockAlign) % soundWaveFormat.nAvgBytesPerSec;
			bytesToWrite = lockCursor > targetCursor ? soundWaveFormat.nAvgBytesPerSec : 0;
			bytesToWrite += targetCursor - lockCursor;
		}

		SoundBuffer soundBuffer = {
			.samples = soundSamples,
			.samplesPerSecond = soundWaveFormat.nSamplesPerSec,
			.samplesCount = bytesToWrite / soundWaveFormat.nBlockAlign,
		};

		GameUpdateAndRender(&screenBuffer, &soundBuffer);
		Win32DisplayScreenBuffer(window, deviceContext);
		Win32FillSoundBuffer(&soundBuffer, lockCursor, bytesToWrite, &runningSampleIndex);

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