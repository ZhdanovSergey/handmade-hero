#include <windows.h>

static HBITMAP bitmapHandle;
static HDC bitmapDeviceContext;
static BITMAPINFO bitmapInfo;
static void *bitmapMemory;

static void Win32ResizeDIBSection(int width, int height) {
	if (bitmapHandle) {
		DeleteObject(bitmapHandle);
	}
	
	if (!bitmapDeviceContext) {
		bitmapDeviceContext = CreateCompatibleDC(0);
	}

	// TODO: how to init const fields once?
	BITMAPINFOHEADER bmiHeader = bitmapInfo.bmiHeader;
	bmiHeader.biSize = sizeof(bmiHeader);
	bmiHeader.biWidth = width;
	bmiHeader.biHeight = height;
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 32;
	bmiHeader.biCompression = BI_RGB;

	bitmapHandle = CreateDIBSection(
		bitmapDeviceContext, &bitmapInfo,
		DIB_RGB_COLORS, &bitmapMemory,
		0, 0
	);
}

static void Win32UpdateWindow(HDC deviceContext, int x, int y, int width, int height) {
	StretchDIBits(
		deviceContext,
		x, y, width, height,
		x, y, width, height,
		bitmapMemory, &bitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

LRESULT Win32MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE: {
			RECT clientRect;
			GetClientRect(window, &clientRect);
			int width = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;
			Win32ResizeDIBSection(width, height);
		} break;

		case WM_PAINT: {
			PAINTSTRUCT paintStruct;
			RECT rcPaint = paintStruct.rcPaint;
			HDC deviceContext = BeginPaint(window, &paintStruct);

			Win32UpdateWindow(
				deviceContext,
				rcPaint.left, rcPaint.top,
				rcPaint.right - rcPaint.left,
				rcPaint.bottom - rcPaint.top
			);

			EndPaint(window, &paintStruct);
		} break;

		// TODO: check why close button not working before we move or resize window
		case WM_CLOSE:
		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;

		default: {
			return DefWindowProcA(window, message, wParam, lParam);
		};
	}

	return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	WNDCLASSA windowClass = {};
	windowClass.lpszClassName = "HandmadeHeroWindowClass";
	windowClass.hInstance = hInstance;
	windowClass.lpfnWndProc = Win32MainWindowCallback;

	RegisterClassA(&windowClass);

	HWND windowHandle = CreateWindowExA(
		0, windowClass.lpszClassName, "Handmade Hero",
		WS_TILEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, hInstance, 0
	);

	while (true) {
		MSG message;
		BOOL messageResult = GetMessageA(&message, windowHandle, 0, 0);

		if (messageResult <= 0) {
			return 0;
		}

		TranslateMessage(&message);
		DispatchMessageA(&message);
	}
}