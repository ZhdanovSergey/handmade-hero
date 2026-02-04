#pragma once

#include "globals.hpp"
#include "handmade.hpp"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

using Direct_Sound_Create = HRESULT WINAPI(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
using Xinput_Get_State = DWORD(DWORD dwUserIndex, XINPUT_STATE *pState);
using Xinput_Set_State = DWORD(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

static const i32 INITIAL_WINDOW_WIDTH = 1280;
static const i32 INITIAL_WINDOW_HEIGHT = 720;
static const UINT SLEEP_GRANULARITY_MS = timeBeginPeriod(1) == TIMERR_NOERROR ? 1U : 0U;
static const i64 PERFORMANCE_FREQUENCY = []{
	LARGE_INTEGER query_result;
	QueryPerformanceFrequency(&query_result);
	return query_result.QuadPart;
}();

static const f32 TARGET_SECONDS_PER_FRAME = []{
	i32 default_frame_rate = 33;
    HDC device_context = GetDC(0);
    i32 refresh_rate = GetDeviceCaps(device_context, VREFRESH);
    ReleaseDC(0, device_context);
	if (refresh_rate == 0 || refresh_rate == 1) return 1.0f / (f32)default_frame_rate;

	default_frame_rate = min(refresh_rate, default_frame_rate);
	if (refresh_rate % default_frame_rate == 0) return 1.0f / (f32)default_frame_rate;

	f32 sync_factor = (f32)(refresh_rate / default_frame_rate) + 1.0f;
	return sync_factor / (f32)refresh_rate;
}();

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow);
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
static void wait_until_end_of_frame(i64 flip_timestamp);
static void get_build_file_path(const char* file_name, char* dest, i32 dest_size);
static FILETIME get_file_write_time(const char* file_name);
static inline f32 get_seconds_elapsed(i64 start);
static inline i64 get_timestamp();

struct Game_Code {
	Game_Code();
	Game::Update_And_Render* update_and_render;
	Game::Get_Sound_Samples* get_sound_samples;
	void dev_reload_if_recompiled();

	private:
	HMODULE dll;
	FILETIME write_time;
	char dll_path[MAX_PATH];
	char temp_dll_path[MAX_PATH];
	void load();
};

struct Input {
	Input();
	Game::Input game_input;
	void process_gamepad();
	void process_keyboard_button(WPARAM key_code, bool is_pressed);
	void dev_process_mouse(HWND window);

	private:
	Xinput_Get_State* XInputGetState;
	Xinput_Set_State* XInputSetState;
	static f32 get_normalized_stick_value(SHORT value, SHORT deadzone);
	static void process_gamepad_button(Game::Button_State& state, bool is_pressed);
};

enum Dev_Replayer_State { Idle, Recording, Playing, Count };

struct Dev_Replayer {
	Dev_Replayer(const Game::Memory& game_memory);
	void next_state(Game::Memory& game_memory, Game::Input& game_input);
	void record_or_replace(Game::Memory& game_memory, Game::Input& game_input);

	private:
	char state_path[MAX_PATH];
	char input_path[MAX_PATH];
	HANDLE state_handle;
	HANDLE input_handle;
	Dev_Replayer_State replayer_state;
	void start_record(const Game::Memory& game_memory);
	void start_play(Game::Memory& game_memory);
	void record(const Game::Input& game_input);
	void play(Game::Memory& game_memory, Game::Input& game_input);
};

struct Screen {
	Screen();
	Game::Screen_Buffer game_screen;
	void resize(i32 width, i32 height);
	void submit(HWND window, HDC device_context) const;
	void dev_draw_vertical(i32 x, i32 top, i32 bottom, u32 color);

	private:
	BITMAPINFO bitmap_info;
};

struct Dev_Sound_Time_Marker {
	DWORD output_play_cursor;
	DWORD output_write_cursor;
	DWORD output_location;
	DWORD output_byte_count;
	DWORD flip_play_cursor;
	DWORD expected_flip_play_cursor;
};

struct Sound {
	Sound(HWND window);
	Game::Sound_Buffer game_sound;
	void calc_samples_to_write(i64 flip_timestamp);
	void submit();
	void dev_draw_sync(Screen& screen);

	private:
	WAVEFORMATEX wave_format;
	IDirectSoundBuffer* buffer;
	DWORD output_location;
	Dev_Sound_Time_Marker dev_markers[32]; // ожидаемый фреймрейт - 1
	i32 dev_markers_index;
	DWORD get_buffer_size() 	const { return wave_format.nAvgBytesPerSec; }
	DWORD get_bytes_per_frame() const { return (DWORD)((f32)wave_format.nAvgBytesPerSec * TARGET_SECONDS_PER_FRAME); }
	DWORD get_safety_bytes() 	const { return get_bytes_per_frame() / 3; }
};