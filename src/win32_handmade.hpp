#include "globals.hpp"
#include "handmade.hpp"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

struct Screen {
	BITMAPINFO bitmapInfo;
	Game::ScreenPixel* memory;

	// biHeight отрицательный чтобы верхний левый пиксель был первым в буфере
	void setHeight(u32 height) 	{ bitmapInfo.bmiHeader.biHeight = - (LONG)height; }
	u32 getHeight() const 		{ return (u32)std::abs(bitmapInfo.bmiHeader.biHeight); }
	void setWidth(u32 width) 	{ bitmapInfo.bmiHeader.biWidth = (LONG)width; }
	u32 getWidth() const 		{ return (u32)bitmapInfo.bmiHeader.biWidth; }
};

struct Sound {
	// TODO: создать отдельную структуру для полей, которые не должны переходить границу фрейма
	WAVEFORMATEX waveFormat;
	IDirectSoundBuffer* buffer;
	u32 runningSampleIndex;
	DWORD outputLocation;
	DWORD outputByteCount;
	DWORD bytesPerFrame;
	DWORD safetyBytes;
	DWORD playCursor;
	DWORD writeCursor;

	DWORD getBufferSize() const { return waveFormat.nAvgBytesPerSec; }
};

struct GameCode {
	Game::UpdateAndRenderType* UpdateAndRender;
	Game::GetSoundSamplesType* GetSoundSamples;
	HMODULE gameDll;
};

namespace Debug {
    struct Marker {
        DWORD outputPlayCursor;
		DWORD outputWriteCursor;
		DWORD outputLocation;
		DWORD outputByteCount;
        DWORD flipPlayCursor;
        DWORD expectedFlipPlayCursor;
    };

    static void SoundSyncDisplay(
		Screen& screen, const Sound& sound,
		const Marker* markers, size_t markersCount, size_t currentMarkerIndex
	);
    static void DrawVertical(Screen& screen, u32 x, u32 top, u32 bottom, u32 color);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int);
static LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
static GameCode LoadGameCode();
static void UnloadGameCode(GameCode& gameCode);
static void DisplayScreenBuffer(HWND window, HDC deviceContext, const Screen& screen);
static void ResizeScreenBuffer(Screen& screen, u32 width, u32 height);
static void ProcessPendingMessages(Game::Controller& controller);
static inline u64 GetWallClock();
static inline f32 GetSecondsElapsed(u64 start);

// Sound
static void InitDirectSound(HWND window, Sound& sound);
static void CalcRequiredSoundOutput(Sound& sound, u64 flipWallClock, Debug::Marker* debugMarkersArray, size_t debugMarkersIndex);
static void ClearSoundBuffer(Sound& sound);
static void FillSoundBuffer(const Game::SoundBuffer& source, Sound& sound);

// ---XInput---
static void LoadXInputLibrary();
static void ProcessGamepadInput(Game::Controller& controller);
static inline void ProcessGamepadButton(Game::ButtonState& state, bool isPressed);
static inline f32 GetNormalizedStickValue(SHORT value, SHORT deadzone);

typedef DWORD XInputGetStateType(DWORD, XINPUT_STATE*);
static 	DWORD XInputGetStateStub(DWORD, XINPUT_STATE*) { return ERROR_DLL_INIT_FAILED; };
static XInputGetStateType* XInputGetStateStubOrFn = XInputGetStateStub;
#define XInputGetState XInputGetStateStubOrFn

typedef DWORD XInputSetStateType(DWORD, XINPUT_VIBRATION*);
static 	DWORD XInputSetStateStub(DWORD, XINPUT_VIBRATION*) { return ERROR_DLL_INIT_FAILED; };
static XInputSetStateType* XInputSetStateStubOrFn = XInputSetStateStub;
#define XInputSetState XInputSetStateStubOrFn
// ------------