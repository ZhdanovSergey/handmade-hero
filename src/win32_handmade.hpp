#include "globals.hpp"
#include "handmade.hpp"

#include <windows.h>
#include <dsound.h>
#include <xinput.h>

struct Screen {
	BITMAPINFO bitmap_info;
	Game::Screen_Pixel* memory;

	// biHeight отрицательный чтобы верхний левый пиксель был первым в буфере
	void set_height(u32 height) { bitmap_info.bmiHeader.biHeight = - (LONG)height; }
	void set_width(u32 width)	{ bitmap_info.bmiHeader.biWidth  =   (LONG)width; }
	u32 get_height() const { return (u32)std::abs(bitmap_info.bmiHeader.biHeight); }
	u32 get_width()	 const { return (u32)		  bitmap_info.bmiHeader.biWidth; }
};

struct Sound {
	// TODO: создать отдельную структуру для полей, которые не должны переходить границу фрейма
	WAVEFORMATEX wave_format;
	IDirectSoundBuffer* buffer;
	u32 running_sample_index;
	DWORD output_location;
	DWORD output_byte_count;
	DWORD bytes_per_frame;
	DWORD safety_bytes;
	DWORD play_cursor;
	DWORD write_cursor;

	DWORD get_buffer_size() const { return wave_format.nAvgBytesPerSec; }
};

struct Game_Code {
	Game::Update_And_Render* update_and_render;
	Game::Get_Sound_Samples* get_sound_samples;
	HMODULE game_dll;
};

namespace Debug {
    struct Marker {
        DWORD output_play_cursor;
		DWORD output_write_cursor;
		DWORD output_location;
		DWORD output_byte_count;
        DWORD flip_play_cursor;
        DWORD expected_flip_play_cursor;
    };

    static void sound_sync_display(
		Screen& screen, const Sound& sound,
		const Marker* markers, uptr markers_count, uptr current_marker_index
	);
    static void draw_vertical(Screen& screen, u32 x, u32 top, u32 bottom, u32 color);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow);
static LRESULT CALLBACK main_window_callback(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
static Game_Code load_game_code();
static void unload_game_code(Game_Code& game_code);
static void display_screen_buffer(HWND window, HDC device_context, const Screen& screen);
static void resize_screen_buffer(Screen& screen, u32 width, u32 height);
static void process_pending_messages(Game::Controller& controller);
static inline u64 get_wall_clock();
static inline f32 get_seconds_elapsed(u64 start);
static u64 get_performance_frequency();

// Sound
static void init_direct_sound(HWND window, Sound& sound);
static void calc_required_sound_output(Sound& sound, u64 flip_wall_clock, Debug::Marker* debug_markers_array, uptr debug_markers_index);
static void clear_sound_buffer(Sound& sound);
static void fill_sound_buffer(const Game::Sound_Buffer& source, Sound& sound);

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