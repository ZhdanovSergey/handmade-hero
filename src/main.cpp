#include <stdint.h>
#include <windows.h>

bool isAppRunning = true;
BITMAPINFO bitmapInfo = {};
int bitmapWidth = 0;
int bitmapHeight = 0;
void *bitmapMemory = nullptr;

void RenderGradient(const int blueOffset, const int greenOffset) {
	auto *pixel = (uint32_t *) bitmapMemory;

	for (auto y = 0; y < bitmapHeight; y++) {
		for (auto x = 0; x < bitmapWidth; x++) {
			uint8_t green = y + greenOffset;
			uint8_t blue = x + blueOffset;
			*pixel++ = (uint32_t) ((green << 8) | blue);
		}
	}
}

void Win32ResizeDIBSection(const RECT *clientRect) {
	if (bitmapMemory) {
		VirtualFree(bitmapMemory, NULL, MEM_RELEASE);
	} else {
		bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;
	}

	bitmapWidth = clientRect->right - clientRect->left;
	bitmapHeight = clientRect->bottom - clientRect->top;

	bitmapInfo.bmiHeader.biWidth = bitmapWidth;
	bitmapInfo.bmiHeader.biHeight = - bitmapHeight;

	const auto bytesPerPixel = 4;
	const auto bitmapMemorySize = bytesPerPixel * bitmapWidth * bitmapHeight;
	bitmapMemory = VirtualAlloc(NULL, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

void Win32UpdateWindow(const HDC deviceContext, const RECT *paintRect) {
	const auto paintWidth = paintRect->right - paintRect->left;
	const auto paintHeight = paintRect->bottom - paintRect->top;

	StretchDIBits(deviceContext,
		NULL, NULL, bitmapWidth, bitmapHeight,
		NULL, NULL, paintWidth, paintHeight,
		bitmapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY
	);
}

LRESULT Win32MainWindowCallback(const HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam) {
	switch (message) {
		case WM_SIZE: {
			RECT clientRect;
			GetClientRect(window, &clientRect);
			Win32ResizeDIBSection(&clientRect);
		} break;

		case WM_PAINT: {
			PAINTSTRUCT paintStruct;
			const auto deviceContext = BeginPaint(window, &paintStruct);
			Win32UpdateWindow(deviceContext, &paintStruct.rcPaint);
			EndPaint(window, &paintStruct);
		} break;

		case WM_CLOSE:
		case WM_DESTROY:
		case WM_QUIT: {
			isAppRunning = false;
		} break;

		default: {
			return DefWindowProcA(window, message, wParam, lParam);
		};
	}

	return NULL;
}

int wWinMain(const HINSTANCE hInstance, const HINSTANCE hPrevInstance, const PWSTR pCmdLine, const int nCmdShow) {
	const WNDCLASSA windowClass = {
		.lpfnWndProc = Win32MainWindowCallback,
		.hInstance = hInstance,
		.lpszClassName = "HandmadeHeroWindowClass",
	};

	RegisterClassA(&windowClass);

	const auto windowName = "Handmade Hero";
	const auto style = WS_TILEDWINDOW | WS_VISIBLE;
	const auto window = CreateWindowExA(
		NULL, windowClass.lpszClassName, windowName, style,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL
	);

	auto blueOffset = 0;

	while (isAppRunning) {
		RenderGradient(blueOffset, 0);
		blueOffset++;

		auto deviceContext = GetDC(window);
		RECT clientRect;
		GetClientRect(window, &clientRect);
		Win32UpdateWindow(deviceContext, &clientRect);
		ReleaseDC(window, deviceContext);

		MSG message;
		if (!PeekMessageA(&message, window, NULL, NULL, PM_REMOVE)) {
			continue;
		}

		TranslateMessage(&message);
		DispatchMessageA(&message);
	}

	return NULL;
}