#include "globals.hpp"
#include "game.hpp"

#include <windows.h>
#include <dsound.h>

typedef HRESULT WINAPI DirectSoundCreateType(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);

struct Screen {
	BITMAPINFO bitmapInfo;
	Game::ScreenPixel* memory;

	// biHeight is negative in order to top-left pixel been first in bitmap
	void setHeight(u32 height) 	{ bitmapInfo.bmiHeader.biHeight = - (LONG)height; }
	u32 getHeight() const 		{ return (u32)std::abs(bitmapInfo.bmiHeader.biHeight); }
	void setWidth(u32 width) 	{ bitmapInfo.bmiHeader.biWidth = (LONG)width; }
	u32 getWidth() const 		{ return (u32)bitmapInfo.bmiHeader.biWidth; }
};

struct Sound {
	WAVEFORMATEX waveFormat;
	IDirectSoundBuffer* buffer;
	u32 runningSampleIndex;
	DWORD outputLocation;
	DWORD outputByteCount;
	DWORD bytesPerFrame;
	DWORD safetyBytes;

	// TODO: deprecated?
	DWORD playCursor;
	DWORD writeCursor;

	DWORD getBufferSize() const     { return waveFormat.nAvgBytesPerSec; }
};

// TODO: attach methods to structs after day 025?
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int);
static LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
static void DisplayScreenBuffer(HWND window, HDC deviceContext, const Screen& screen);
static void ResizeScreenBuffer(Screen& screen, u32 width, u32 height);
static void InitDirectSound(HWND window, Sound& sound);
static void ClearSoundBuffer(Sound& sound);
static void FillSoundBuffer(const Game::SoundBuffer& source, Sound& sound);
static void ProcessPendingMessages(Game::Input& gameInput);
static inline u64 GetWallClock();
static inline f32 GetSecondsElapsed(u64 start);

namespace Debug {
    struct Marker {
        DWORD outputPlayCursor;
		DWORD outputWriteCursor;
		DWORD outputLocation;
		DWORD outputByteCount;
        DWORD flipPlayCursor;
        DWORD expectedFlipPlayCursor;
    };

    static void SyncDisplay(
		Screen& screen, const Sound& sound,
		const Marker* markers, size_t markersCount, size_t currentMarkerIndex
	);
    static void DrawVertical(Screen& screen, u32 x, u32 top, u32 bottom, u32 color);
}