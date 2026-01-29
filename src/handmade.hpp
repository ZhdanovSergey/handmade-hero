#pragma once

#include "globals.hpp"

namespace Game {
	struct Game_State {
		f32 t_sine;
		u32 tone_hz;
		u32 player_x, player_y;
		u32 player_size;
		u32 player_velocity;
	};

	struct Button_State {
		bool is_pressed;
		u32 transitions_count;
	};

	struct Controller {
		bool is_connected;
		bool is_analog;

		f32 start_x, start_y;
		f32 end_x, end_y;
		f32 average_x, average_y;
		f32 min_x, min_y;
		f32 max_x, max_y;

		Button_State start, back;
		Button_State left_shoulder, right_shoulder;
		Button_State move_up, move_down, move_left, move_right;
		Button_State action_up, action_down, action_left, action_right;
	};

	struct Dev_Mouse {
		Button_State left_button;
		Button_State right_button;
		i32 x, y;
	};

	struct Input {
		Controller controllers[2];
		Dev_Mouse dev_mouse;
	};

	struct Memory {
		bool is_initialized;
		usize permanent_size;
		usize transient_size;
		u8* permanent_storage;
		u8* transient_storage;
    	Platform::Read_File_Sync* read_file_sync;
    	Platform::Write_File_Sync* write_file_sync;
    	Platform::Free_File_Memory* free_file_memory;

		usize get_total_size() const { return permanent_size + transient_size; }
	};

	struct Screen_Buffer {
		u32 width, height;
		u32* memory;
	};

	struct Sound_Sample {
		i16 left, right;
	};

	struct Sound_Buffer {
		u32 samples_per_second;
		u32 samples_to_write;
		Sound_Sample* samples;
	};

	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer);
	using Update_And_Render = decltype(update_and_render);

	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer);
	using Get_Sound_Samples = decltype(get_sound_samples);

	static void render_gradient(const Game_State& game_state, Screen_Buffer& screen_buffer);
	static void render_player(const Game_State& game_state, Screen_Buffer& screen_buffer);
	static void render_rectangle(Screen_Buffer& screen_buffer, i32 x, i32 y, i32 width, i32 height, u32 color);
}