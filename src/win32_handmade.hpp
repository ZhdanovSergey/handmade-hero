#pragma once

#include "globals.hpp"
#include "game.hpp"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

using Direct_Sound_Create = HRESULT WINAPI(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
using Xinput_Get_State = DWORD(DWORD dwUserIndex, XINPUT_STATE *pState);
using Xinput_Set_State = DWORD(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

static i64 get_perf_frequency();
static f32 get_target_seconds_per_frame();
static const i32 INITIAL_WINDOW_WIDTH = 1280;
static const i32 INITIAL_WINDOW_HEIGHT = 720;
static const i64 PERF_FREQUENCY = get_perf_frequency();
static const f32 SLEEP_GRANULARITY_SECONDS = (f32)(timeBeginPeriod(1) == TIMERR_NOERROR) / 1000.0f;
static const f32 TARGET_SECONDS_PER_FRAME = get_target_seconds_per_frame();

struct Game_Code {
	HMODULE dll;
	FILETIME write_time;
	char dll_path[MAX_PATH];
	char temp_dll_path[MAX_PATH];
	Game::Update_And_Render* update_and_render;
	Game::Get_Sound_Samples* get_sound_samples;
};

struct Input {
	Game::Input game_input;
	Xinput_Get_State* XInputGetState;
	Xinput_Set_State* XInputSetState;
};

enum Dev_Replayer_State { Idle, Recording, Playing, Count };

// TODO: возможно стоит избавиться от префиксов dev_
struct Dev_Replayer {
	char state_path[MAX_PATH];
	char input_path[MAX_PATH];
	HANDLE state_handle;
	HANDLE input_handle;
	Dev_Replayer_State replayer_state;
};

struct Screen {
	Game::Screen game_screen;
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
	Game::Sound game_sound;
	WAVEFORMATEX wave_format;
	IDirectSoundBuffer* buffer;
	DWORD output_location;
	Dev_Sound_Time_Marker dev_markers[32]; // ожидаемый фреймрейт - 1
	i32 dev_markers_index;

	// TODO: обдумать использование полей структуры вместо геттеров
	DWORD get_buffer_size() 	const { return wave_format.nAvgBytesPerSec; }
	DWORD get_bytes_per_frame() const { return (DWORD)((f32)wave_format.nAvgBytesPerSec * TARGET_SECONDS_PER_FRAME); }
	DWORD get_safety_bytes() 	const { return get_bytes_per_frame() / 3; }
};

static void calc_sound_samples_to_write(Sound& sound, i64 flip_timestamp);
static void dev_draw_vertical_line(Screen& screen, i32 x, i32 top, i32 bottom, u32 color);
static void dev_process_mouse_input(Input& input, HWND window);
static void dev_reload_game_code_if_recompiled(Game_Code& game_code);
static void dev_draw_sound_sync(Sound& sound, Screen& screen);
static void get_build_file_path(span<const char> file_name, span<char> dest);
static FILETIME get_file_write_time(const char* file_name);
static f32 get_normalized_stick_value(SHORT value);
static f32 get_seconds_elapsed(i64 start);
static i64 get_timestamp();
static Game_Code init_game_code();
static Input init_input();
static Dev_Replayer init_replayer(const Game::Memory& game_memory);
static Screen init_screen();
static Sound init_sound(HWND window);
static void load_game_code(Game_Code& game_code);
static void process_gamepad_input(Input& input);
static void process_gamepad_button(Game::Button& state, bool is_pressed);
static void process_keyboard_button(Input& input, WPARAM key_code, bool is_pressed);
static void submit_screen(const Screen& screen, HWND window, HDC device_context);
static void submit_sound(Sound& sound);
static void replayer_next_state(Dev_Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input);
static void replayer_play(Dev_Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input);
static void replayer_record(Dev_Replayer& replayer, const Game::Input& game_input);
static void replayer_record_or_replace(Dev_Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input);
static void replayer_start_play(Dev_Replayer& replayer, Game::Memory& game_memory);
static void replayer_start_record(Dev_Replayer& replayer, const Game::Memory& game_memory);
static void reset_game_input_counters(Game::Input& game_input);
static void resize_screen(Screen& screen, i32 width, i32 height);
static void wait_until_end_of_frame(i64 flip_timestamp);
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);