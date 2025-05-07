#include <stdint.h>
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

	int getWidth() {
		return this->bitmapInfo.bmiHeader.biWidth;
	}

	void setWidth(int width) {
		this->bitmapInfo.bmiHeader.biWidth = width;
	}

	int getHeight() {
		return - this->bitmapInfo.bmiHeader.biHeight;
	}

	void setHeight(int height) {
		this->bitmapInfo.bmiHeader.biHeight = - height;
	}

	u_int getMemorySize() {
		int bytesPerPixel = this->bitmapInfo.bmiHeader.biBitCount / 8;
		return (u_int) abs(bytesPerPixel * this->getWidth() * this->getHeight());
	}
};

bool isAppRunning = true;
ScreenBuffer screenBuffer = {};

void InitDirectSound(HWND window) {
	const u_int numberOfChannels = 2;
	const u_int bitsPerSample = 16;
	const u_int samplesPerSecond = 48000;
	constexpr u_int blockAlign = numberOfChannels * bitsPerSample / 8;
	constexpr u_int bytesPerSecond = samplesPerSecond * blockAlign;

	HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");

	if (!dSoundLibrary) {
		return;
	}

	typedef HRESULT DirectSoundCreateType(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
	#pragma warning(suppress: 4191)
	auto directSoundCreate = (DirectSoundCreateType*) GetProcAddress(dSoundLibrary, "DirectSoundCreate");

	if (!directSoundCreate) {
		return;
	}

	LPDIRECTSOUND directSound;
	if (!SUCCEEDED(directSoundCreate(nullptr, &directSound, 0))) {
		return;
	}

	directSound->SetCooperativeLevel(window, DSSCL_PRIORITY);

	WAVEFORMATEX waveFormat = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = numberOfChannels,
		.nSamplesPerSec = samplesPerSecond,
		.nAvgBytesPerSec = bytesPerSecond,
		.nBlockAlign = blockAlign,
		.wBitsPerSample = bitsPerSample
	};

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

	DSBUFFERDESC secondaryBufferDesc = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwBufferBytes = bytesPerSecond,
		.lpwfxFormat = &waveFormat
	};

	LPDIRECTSOUNDBUFFER secondaryBuffer;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&secondaryBufferDesc, &secondaryBuffer, 0))) {
		return;
	};
}

void RenderGradient(int blueOffset, int greenOffset) {
	auto pixel = (uint32_t*) screenBuffer.memory;

	for (int y = 0; y < screenBuffer.getHeight(); y++) {
		for (int x = 0; x < screenBuffer.getWidth(); x++) {
			auto green = (uint8_t) (y + greenOffset);
			auto blue = (uint8_t) (x + blueOffset);
			*pixel++ = (uint32_t) ((green << 8) | blue);
		}
	}
}

void ResizeScreenBuffer(int width, int height) {
	if (screenBuffer.memory) {
		VirtualFree(screenBuffer.memory, 0, MEM_RELEASE);
	}

	screenBuffer.setWidth(width);
	screenBuffer.setHeight(height);
	screenBuffer.memory = VirtualAlloc(nullptr, screenBuffer.getMemorySize(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void DisplayScreenBuffer(HWND window, HDC deviceContext) {
	RECT clientRect;
	GetClientRect(window, &clientRect);

	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;

	StretchDIBits(deviceContext,
		0, 0, clientWidth, clientHeight,
		0, 0, screenBuffer.getWidth(), screenBuffer.getHeight(),
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
			bool isKeyDown = !(lParam & (1 << 31));
			bool wasKeyDown = lParam & (1 << 30);

			if (!isKeyDown || wasKeyDown) {
				return 0;
			}

			switch (wParam) {
				case VK_UP: {
					OutputDebugStringA("VK_UP \n");
				} break;

				case VK_DOWN: {
					OutputDebugStringA("VK_DOWN \n");
				} break;

				case VK_LEFT: {
					OutputDebugStringA("VK_LEFT \n");
				} break;

				case VK_RIGHT: {
					OutputDebugStringA("VK_RIGHT \n");
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

int wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
	const int windowWidth = 1280;
	const int windowHeight = 720;

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
	int blueOffset = 0;

	while (isAppRunning) {		
		MSG message;
		while (PeekMessageA(&message, window, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
		
		RenderGradient(blueOffset, 0);
		DisplayScreenBuffer(window, deviceContext);
		blueOffset++;
	}

	return 0;
}