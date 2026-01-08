#pragma once

#include "globals.hpp"
#include "handmade.hpp"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

static const u32 INITIAL_WINDOW_WIDTH = 1280;
static const u32 INITIAL_WINDOW_HEIGHT = 720;
static const u32 TARGET_UPDATE_FREQUENCY = 30;
static const f32 TARGET_SECONDS_PER_FRAME = 1.0f / (f32)TARGET_UPDATE_FREQUENCY;
static const UINT SLEEP_GRANULARITY_MS = timeBeginPeriod(1) == TIMERR_NOERROR ? 1 : 0;
static const u64 PERFORMANCE_FREQUENCY = []() -> u64 {
	LARGE_INTEGER query_result;
	QueryPerformanceFrequency(&query_result);
	return (u64)query_result.QuadPart;
}();

struct Debug_Marker {
	DWORD output_play_cursor;
	DWORD output_write_cursor;
	DWORD output_location;
	DWORD output_byte_count;
	DWORD flip_play_cursor;
	DWORD expected_flip_play_cursor;
};

struct Screen {
	u32* memory;
	u32 get_width()	 const { return (u32)	bitmap_info.bmiHeader.biWidth; }
	u32 get_height() const { return (u32) - bitmap_info.bmiHeader.biHeight; }
	void resize(u32 width, u32 height);
	void display(HWND window, HDC device_context) const;

	private:
	BITMAPINFO bitmap_info = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biPlanes = 1,
			.biBitCount = sizeof(*memory) * 8,
			.biCompression = BI_RGB,
		}
	};
};

struct Sound {
	WAVEFORMATEX wave_format = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = 48'000,
		.nAvgBytesPerSec = sizeof(Game::Sound_Sample) * wave_format.nSamplesPerSec,
		.nBlockAlign = sizeof(Game::Sound_Sample),
		.wBitsPerSample = sizeof(Game::Sound_Sample) / wave_format.nChannels * 8,
	};
	IDirectSoundBuffer* buffer;
	DWORD output_byte_count;
	DWORD get_buffer_size() 	const { return wave_format.nAvgBytesPerSec; }
	DWORD get_safety_bytes() 	const { return get_bytes_per_frame() / 3; }
	void calc_required_output(u64 flip_wall_clock, Debug_Marker* debug_markers_array, uptr debug_markers_index);
	void fill(const Game::Sound_Buffer& source);
	void play(HWND window);

	private:
	u32 running_sample_index;
	DWORD get_bytes_per_frame() const { return wave_format.nAvgBytesPerSec / TARGET_UPDATE_FREQUENCY; }
	DWORD get_output_location()	const { return running_sample_index * wave_format.nBlockAlign % get_buffer_size(); }
};

struct Game_Code {
	Game::Update_And_Render* update_and_render;
	Game::Get_Sound_Samples* get_sound_samples;
	Game_Code();
	void load();
	void reload_if_recompiled();

	private:
	HMODULE dll;
	FILETIME write_time;
	char dll_path[MAX_PATH];
	char temp_dll_path[MAX_PATH];
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow);
static LRESULT CALLBACK main_window_callback(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
static void process_pending_messages(Game::Controller& controller);
static void wait_until_end_of_frame(u64 flip_wall_clock);
static FILETIME get_file_write_time(const char* filename);
static inline f32 get_seconds_elapsed(u64 start);
static inline u64 get_wall_clock();

static void debug_draw_vertical(Screen& screen, u32 x, u32 top, u32 bottom, u32 color);
static void debug_sound_sync_display(
	Screen& screen, const Sound& sound,
	const Debug_Marker* markers, uptr markers_count, uptr current_marker_index
);

// ---XInput---
static void init_xinput();
static void process_gamepad_input(Game::Controller& controller);
static inline void process_gamepad_button(Game::Button_State& state, bool is_pressed);
static inline f32 get_normalized_stick_value(SHORT value, SHORT deadzone);

typedef DWORD Xinput_Get_State		(DWORD dwUserIndex, XINPUT_STATE *pState);
static 	DWORD xinput_get_state_stub	(DWORD dwUserIndex, XINPUT_STATE *pState) { return ERROR_DLL_INIT_FAILED; };
static Xinput_Get_State* xinput_get_state_stub_or_fn = xinput_get_state_stub;
#define XInputGetState xinput_get_state_stub_or_fn

typedef DWORD Xinput_Set_State		(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);
static 	DWORD xinput_set_state_stub	(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration) { return ERROR_DLL_INIT_FAILED; };
static Xinput_Set_State* xinput_set_state_stub_or_fn = xinput_set_state_stub;
#define XInputSetState xinput_set_state_stub_or_fn
// ------------