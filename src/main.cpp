#include <windows.h>

LRESULT MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {		
		case WM_PAINT: {
			PAINTSTRUCT paintStruct;
			HDC deviceContext = BeginPaint(window, &paintStruct);

			RECT rcPaint = paintStruct.rcPaint;
			PatBlt(
				deviceContext,
				rcPaint.left,
				rcPaint.top,
				rcPaint.right - rcPaint.left,
				rcPaint.bottom - rcPaint.top,
				BLACKNESS
			);

			EndPaint(window, &paintStruct);
		} break;

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
	windowClass.lpfnWndProc = MainWindowCallback;

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

		if (messageResult > 0) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
		} else {
			break;
		}
	}

	return 0;
}