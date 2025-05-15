#include <windows.h>
#include <dsound.h>

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

	void setWidth(u_int width) {
		bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(width);
	}

	u_int getWidth() {
		return static_cast<u_int>(bitmapInfo.bmiHeader.biWidth);
	}

	void setHeight(u_int height) {
		// biHeight is negative in order to top-left pixel been first in bitmap
		bitmapInfo.bmiHeader.biHeight = - static_cast<LONG>(height);
	}

	u_int getHeight() {
		LONG positiveHeight = abs(bitmapInfo.bmiHeader.biHeight);
		return static_cast<u_int>(positiveHeight);
	}

	u_int getMemorySize() {
		u_int bytesPerPixel = bitmapInfo.bmiHeader.biBitCount / 8u;
		return bytesPerPixel * getWidth() * getHeight();
	}
};

bool isAppRunning = true;
ScreenBuffer screenBuffer = {};

const u_int soundSamplesPerSecond = 48000;
constexpr u_int soundBytesPerSample = sizeof(short) * 2;
constexpr u_int soundBufferSize = soundSamplesPerSecond * soundBytesPerSample;
IDirectSoundBuffer* soundBuffer = nullptr;

// I`m trying to find bug here, so some vars declared as static for now
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

	LPDIRECTSOUND directSound;
	if (!SUCCEEDED(directSoundCreate(nullptr, &directSound, 0))) {
		return;
	}

	directSound->SetCooperativeLevel(window, DSSCL_PRIORITY);

	// fields order does not match dependecies order
	static WAVEFORMATEX waveFormat = {};
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = 2;
	waveFormat.nSamplesPerSec = soundSamplesPerSecond;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8u;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

	DSBUFFERDESC primaryBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};

	LPDIRECTSOUNDBUFFER primaryBuffer;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&primaryBufferDesc, &primaryBuffer, 0))) {
		return;
	};

	if (!SUCCEEDED(primaryBuffer->SetFormat(&waveFormat))) {
		return;
	}

	static DSBUFFERDESC soundBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwBufferBytes = soundBufferSize,
		.lpwfxFormat = &waveFormat
	};

	if (!SUCCEEDED(directSound->CreateSoundBuffer(&soundBufferDesc, &soundBuffer, 0))) {
		return;
	};
}

void RenderGradient(u_int blueOffset, u_int greenOffset) {
	u_int* pixel = static_cast<u_int*>(screenBuffer.memory);

	for (u_int y = 0; y < screenBuffer.getHeight(); y++) {
		for (u_int x = 0; x < screenBuffer.getWidth(); x++) {
			u_int green = (y + greenOffset) & UCHAR_MAX;
			u_int blue = (x + blueOffset) & UCHAR_MAX;
			*pixel++ = (green << 8) | blue; // padding red green blue
		}
	}
}

void ResizeScreenBuffer(u_int width, u_int height) {
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
	const u_int windowWidth = 1280;
	const u_int windowHeight = 720;

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

	const u_int soundFrequency = 250;
	const short soundVolume = 1000;
	constexpr u_int soundWavePeriod = soundSamplesPerSecond / soundFrequency;
	constexpr u_int soundWaveHalfPeriod = soundWavePeriod / 2;

	bool isSoundPlaying = false;
	u_int runningSampleIndex = 0;
	u_int blueOffset = 0;

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

		DWORD byteToLock = runningSampleIndex * soundBytesPerSample % soundBufferSize;
		DWORD bytesToWrite = 0;

		if (byteToLock == playCursor) {
			bytesToWrite = soundBufferSize;
		}else if (byteToLock > playCursor) {
			bytesToWrite = soundBufferSize - byteToLock;
			bytesToWrite += playCursor;
		} else {
			bytesToWrite = playCursor - byteToLock;
		}

		void* region1;
		DWORD region1Size;
		void* region2;
		DWORD region2Size;

		HRESULT lockResult = soundBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0);
		if (!SUCCEEDED(lockResult)) {
			continue;
		}

		short* sampleOut = static_cast<short*>(region1);
		u_int region1SizeSamples = region1Size / soundBytesPerSample;

		for (u_int sampleIndex = 0; sampleIndex < region1SizeSamples; sampleIndex++) {
			short sampleValue = ((runningSampleIndex / soundWaveHalfPeriod) % 2) ? soundVolume : - soundVolume;
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			runningSampleIndex++;
		}

		sampleOut = static_cast<short*>(region2);
		u_int region2SizeSamples = region2Size / soundBytesPerSample;

		for (u_int sampleIndex = 0; sampleIndex < region2SizeSamples; sampleIndex++) {
			short sampleValue = ((runningSampleIndex / soundWaveHalfPeriod) % 2) ? soundVolume : - soundVolume;
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			runningSampleIndex++;
		}

		soundBuffer->Unlock(region1, region1Size, region2, region2Size);

		if (!isSoundPlaying) {
			isSoundPlaying = true;
			// TODO: address sanitizer catch some problems after Play call, but I can`t fix it by now
			soundBuffer->Play(0, 0, DSBPLAY_LOOPING);
		}
	}

	return 0;
}