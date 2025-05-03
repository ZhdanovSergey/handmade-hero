#include <stdint.h>
#include <windows.h>

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
		return abs(this->bitmapInfo.bmiHeader.biHeight);
	}

	void setHeight(int height) {
		this->bitmapInfo.bmiHeader.biHeight = - height;
	}

	int getMemorySize() {
		const int bytesPerPixel = this->bitmapInfo.bmiHeader.biBitCount / 8;
		return bytesPerPixel * this->getWidth() * this->getHeight();
	}
};

bool isAppRunning = true;
ScreenBuffer screenBuffer = {};

void RenderGradient(const int blueOffset, const int greenOffset) {
	auto pixel = (uint32_t*) screenBuffer.memory;

	for (int y = 0; y < screenBuffer.getHeight(); y++) {
		for (int x = 0; x < screenBuffer.getWidth(); x++) {
			uint32_t green = (uint8_t) (y + greenOffset);
			uint32_t blue = (uint8_t) (x + blueOffset);
			*pixel++ = ((green << 8) | blue);
		}
	}
}

void ResizeScreenBuffer(const int width, const int height) {
	if (screenBuffer.memory) {
		VirtualFree(screenBuffer.memory, 0, MEM_RELEASE);
	}

	screenBuffer.setWidth(width);
	screenBuffer.setHeight(height);
	screenBuffer.memory = VirtualAlloc(nullptr, screenBuffer.getMemorySize(), MEM_COMMIT, PAGE_READWRITE);
}

void DisplayScreenBuffer(const HWND window, const HDC deviceContext) {
	RECT clientRect;
	GetClientRect(window, &clientRect);
	const int windowWidth = clientRect.right - clientRect.left;
	const int windowHeight = clientRect.bottom - clientRect.top;
	
	StretchDIBits(deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, screenBuffer.getWidth(), screenBuffer.getHeight(),
		screenBuffer.memory, &screenBuffer.bitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

LRESULT MainWindowCallback(const HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			const HDC deviceContext = BeginPaint(window, &paint);
			DisplayScreenBuffer(window, deviceContext);
			EndPaint(window, &paint);
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

int wWinMain(const HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
	const WNDCLASSA windowClass = {
		.style = CS_HREDRAW | CS_OWNDC | CS_VREDRAW,
		.lpfnWndProc = MainWindowCallback,
		.hInstance = hInstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	RegisterClassA(&windowClass);

	const int windowWidth = 1280;
	const int windowHeight = 720;

	const auto windowName = "Handmade Hero";
	const auto style = WS_TILEDWINDOW | WS_VISIBLE;
	const HWND window = CreateWindowExA(
		0, windowClass.lpszClassName, windowName, style,
		CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
		nullptr, nullptr, hInstance, nullptr
	);

	const HDC deviceContext = GetDC(window);
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