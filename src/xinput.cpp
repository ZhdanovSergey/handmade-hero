// #include <windows.h>
// #include <xinput.h>

// typedef DWORD XInputGetStateType(DWORD userIndex, XINPUT_STATE* state);
// typedef DWORD XInputSetStateType(DWORD userIndex, XINPUT_VIBRATION* vibration);

// DWORD XInputGetStateStub(DWORD, XINPUT_STATE*) {
// 	return ERROR_DEVICE_NOT_CONNECTED;
// }

// DWORD XInputSetStateStub(DWORD, XINPUT_VIBRATION*) {
// 	return ERROR_DEVICE_NOT_CONNECTED;
// }

// XInputGetStateType* XInputGetState_ = XInputGetStateStub;
// XInputSetStateType* XInputSetState_ = XInputSetStateStub;

// #define XInputGetState XInputGetState_
// #define XInputSetState XInputSetState_

// void LoadXInputLibrary() {
// 	HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll");

//     if (!xInputLibrary) {
//         xInputLibrary = LoadLibraryA("xinput1_3.dll"); // for win7 and lower
//     }

//     if (!xInputLibrary) {
//         return;
//     }

// 	XInputGetState = (XInputGetStateType*) GetProcAddress(xInputLibrary, "XInputGetState");
// 	XInputSetState = (XInputSetStateType*) GetProcAddress(xInputLibrary, "XInputSetState");
// }

// // initialization
// LoadXInputLibrary();

// // inside game loop
// for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++ ) {
// 	XINPUT_STATE controllerState;
// 	XInputGetState(controllerIndex, &controllerState);
// 	XINPUT_GAMEPAD* gamepad = &controllerState.Gamepad;

// 	bool up = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
// 	bool down = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
// 	bool left = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
// 	bool right = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
// }