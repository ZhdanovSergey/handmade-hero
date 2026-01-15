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
static const u64 PERFORMANCE_FREQUENCY = []{
	LARGE_INTEGER query_result;
	QueryPerformanceFrequency(&query_result);
	return (u64)query_result.QuadPart;
}();

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow);
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
static void wait_until_end_of_frame(u64 flip_timestamp);
static FILETIME get_file_write_time(const char* filename);
static inline f32 get_seconds_elapsed(u64 start);
static inline u64 get_timestamp();

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

using Xinput_Get_State = DWORD(DWORD dwUserIndex, XINPUT_STATE *pState);
using Xinput_Set_State = DWORD(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

struct Input {
	Input();
	Game::Input game_input;
	void prepare_for_new_frame();
	void process_gamepad();
	void process_keyboard_button(WPARAM key_code, bool is_pressed);

	private:
	Xinput_Get_State* XInputGetState;
	Xinput_Set_State* XInputSetState;
	static f32 get_normalized_stick_value(SHORT value, SHORT deadzone);
	static void process_gamepad_button(Game::Button_State& state, bool is_pressed);
};

struct Dev_Replayer {
	Dev_Replayer();
	void next_state(Game::Memory& game_memory, Game::Input& game_input);
	void record_or_replace(Game::Memory& game_memory, Game::Input& game_input);

	private:
	enum State { Idle, Recording, Playing, Count };
	State state;
	const char* filename;
	HANDLE record_handle;
	HANDLE play_handle;
	void start_record(const Game::Memory& game_memory);
	void start_play(Game::Memory& game_memory);
	void record(const Game::Input& game_input);
	void play(Game::Memory& game_memory, Game::Input& game_input);
};

struct Screen {
	Screen();
	Game::Screen_Buffer game_screen;
	void resize(u32 width, u32 height);
	void submit(HWND window, HDC device_context) const;
	void dev_draw_vertical(u32 x, u32 top, u32 bottom, u32 color);

	private:
	BITMAPINFO bitmap_info;
};

struct Dev_Marker {
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
	void calc_samples_to_write(u64 flip_timestamp);
	void submit();
	void dev_draw_sync(Screen& screen);

	private:
	WAVEFORMATEX wave_format;
	IDirectSoundBuffer* buffer;
	DWORD running_sample_index;
	Dev_Marker dev_markers[TARGET_UPDATE_FREQUENCY - 1];
	uptr dev_markers_index;
	DWORD get_buffer_size() 	const { return wave_format.nAvgBytesPerSec; }
	DWORD get_bytes_per_frame() const { return wave_format.nAvgBytesPerSec / TARGET_UPDATE_FREQUENCY; }
	DWORD get_output_location()	const { return running_sample_index * wave_format.nBlockAlign % get_buffer_size(); }
	DWORD get_safety_bytes() 	const { return get_bytes_per_frame() / 3; }
};