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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow);
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
static void wait_until_end_of_frame(u64 flip_wall_clock);
static FILETIME get_file_write_time(const char* filename);
static inline f32 get_seconds_elapsed(u64 start);
static inline u64 get_wall_clock();

struct Game_Code {
	Game_Code();
	Game::Update_And_Render* update_and_render = Game::update_and_render_stub;
	Game::Get_Sound_Samples* get_sound_samples = Game::get_sound_samples_stub;
	void debug_reload_if_recompiled();

	private:
	HMODULE dll = {};
	FILETIME write_time = {};
	char dll_path[MAX_PATH] = {};
	char temp_dll_path[MAX_PATH] = {};
	void load();
};

struct Input {
	Input();
	Game::Input game_input = {};
	void prepare_for_new_frame();
	void process_gamepad();
	void process_keyboard_button(WPARAM key_code, bool is_pressed);

	private:
	typedef DWORD Xinput_Get_State(DWORD dwUserIndex, XINPUT_STATE *pState);
	typedef DWORD Xinput_Set_State(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);
	Xinput_Get_State* XInputGetState = [](auto...) { return (DWORD)ERROR_DLL_INIT_FAILED; };
	Xinput_Set_State* XInputSetState = [](auto...) { return (DWORD)ERROR_DLL_INIT_FAILED; };
	static f32 get_normalized_stick_value(SHORT value, SHORT deadzone);
	static void process_gamepad_button(Game::Button_State& state, bool is_pressed);
};

struct Screen {
	Screen();
	Game::Screen_Buffer game_buffer = {};
	void resize(u32 width, u32 height);
	void display(HWND window, HDC device_context) const;
	void debug_draw_vertical(u32 x, u32 top, u32 bottom, u32 color);

	private:
	BITMAPINFO bitmap_info = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biPlanes = 1,
			.biBitCount = sizeof(*game_buffer.memory) * 8,
			.biCompression = BI_RGB,
		}
	};
};

struct Debug_Marker {
	DWORD output_play_cursor;
	DWORD output_write_cursor;
	DWORD output_location;
	DWORD output_byte_count;
	DWORD flip_play_cursor;
	DWORD expected_flip_play_cursor;
};

struct Sound {
	Sound(HWND window);
	Game::Sound_Buffer game_buffer = {};
	void calc_samples_to_write(u64 flip_wall_clock);
	void submit();
	void debug_sync_display(Screen& screen);

	private:
	WAVEFORMATEX wave_format = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = 48'000,
		.nAvgBytesPerSec = sizeof(Game::Sound_Sample) * wave_format.nSamplesPerSec,
		.nBlockAlign 	 = sizeof(Game::Sound_Sample),
		.wBitsPerSample  = sizeof(Game::Sound_Sample) / wave_format.nChannels * 8,
	};
	IDirectSoundBuffer* buffer = {};
	DWORD running_sample_index = {};
	Debug_Marker debug_markers[TARGET_UPDATE_FREQUENCY - 1] = {};
	uptr debug_markers_index = {};
	DWORD get_buffer_size() 	const { return wave_format.nAvgBytesPerSec; }
	DWORD get_bytes_per_frame() const { return wave_format.nAvgBytesPerSec / TARGET_UPDATE_FREQUENCY; }
	DWORD get_output_location()	const { return running_sample_index * wave_format.nBlockAlign % get_buffer_size(); }
	DWORD get_safety_bytes() 	const { return get_bytes_per_frame() / 3; }
};