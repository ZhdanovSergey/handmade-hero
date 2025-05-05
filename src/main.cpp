#include <stdint.h>
#include <windows.h>

// GAMEPAD INPUT
// -------------------------------------------------------------------------------------------
// #include <xinput.h>

// typedef DWORD XInputGetStateType(DWORD userIndex, XINPUT_STATE* state);
// typedef DWORD XInputSetStateType(DWORD userIndex, XINPUT_VIBRATION* vibration);

// DWORD XInputGetStateStub(DWORD, XINPUT_STATE*) {
// 	return 0;
// }

// DWORD XInputSetStateStub(DWORD, XINPUT_VIBRATION*) {
// 	return 0;
// }

// XInputGetStateType* XInputGetState_ = XInputGetStateStub;
// XInputSetStateType* XInputSetState_ = XInputSetStateStub;

// #define XInputGetState XInputGetState_
// #define XInputSetState XInputSetState_

// void LoadXInputLibrary() {
// 	HMODULE xInputLibrary = LoadLibraryA("xinput1_3.dll");
// 	XInputGetState = (XInputGetStateType*) GetProcAddress(xInputLibrary, "XInputGetState");
// 	XInputSetState = (XInputSetStateType*) GetProcAddress(xInputLibrary, "XInputSetState");
// }
// -------------------------------------------------------------------------------------------

struct ScreenBuffer {
	BITMAPINFO bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	};

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

	int getMemorySize() {
		int bytesPerPixel = this->bitmapInfo.bmiHeader.biBitCount / 8;
		return bytesPerPixel * this->getWidth() * this->getHeight();
	}
};

bool isAppRunning = true;
ScreenBuffer screenBuffer = {};

void RenderGradient(int blueOffset, int greenOffset) {
	auto pixel = (uint32_t*) screenBuffer.memory;

	for (int y = 0; y < screenBuffer.getHeight(); y++) {
		for (int x = 0; x < screenBuffer.getWidth(); x++) {
			auto green = (uint8_t) (y + greenOffset);
			auto blue = (uint8_t) (x + blueOffset);
			*pixel++ = ((green << 8) | blue);
		}
	}
}

void ResizeScreenBuffer(int width, int height) {
	if (screenBuffer.memory) {
		VirtualFree(screenBuffer.memory, 0, MEM_RELEASE);
	}

	screenBuffer.setWidth(width);
	screenBuffer.setHeight(height);
	screenBuffer.memory = VirtualAlloc(nullptr, screenBuffer.getMemorySize(), MEM_COMMIT, PAGE_READWRITE);
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
		case WM_KEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN: {
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
	WNDCLASSA windowClass = {
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = MainWindowCallback,
		.hInstance = hInstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	RegisterClassA(&windowClass);

	int windowWidth = 1280;
	int windowHeight = 720;
	LPCSTR windowName = "Handmade Hero";
	DWORD style = WS_TILEDWINDOW | WS_VISIBLE;

	HWND window = CreateWindowExA(
		0, windowClass.lpszClassName, windowName, style,
		CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
		nullptr, nullptr, hInstance, nullptr
	);

	HDC deviceContext = GetDC(window);
	ResizeScreenBuffer(windowWidth, windowHeight);
	int blueOffset = 0;

	// GAMEPAD INPUT
	// -------------------------------------------------------------------------------------------
	// LoadXInputLibrary();
	// -------------------------------------------------------------------------------------------

	while (isAppRunning) {
		// GAMEPAD INPUT
		// -------------------------------------------------------------------------------------------
		// for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++ ) {
		// 	XINPUT_STATE controllerState;
		// 	XInputGetState(controllerIndex, &controllerState);
		// 	XINPUT_GAMEPAD* gamepad = &controllerState.Gamepad;

		// 	bool up = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
		// 	bool down = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
		// 	bool left = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
		// 	bool right = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
		// }
		// -------------------------------------------------------------------------------------------
		
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