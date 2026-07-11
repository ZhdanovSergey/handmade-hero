#pragma once

#include "globals.hpp"
#include "tiles.hpp"

namespace Game {
	static const i32 SCENE_WIDTH_TILES = 17;
	static const i32 SCENE_HEIGHT_TILES = 9;
	static const i32 SCENES_PER_SCREEN = 1;

	struct Controller_Button {
		i32 transitions_count;
		bool is_pressed;
	};

	struct Controller {
		Controller_Button start, back;
		Controller_Button left_shoulder, right_shoulder;
		Controller_Button move_up, move_down, move_left, move_right;
		Controller_Button action_up, action_down, action_left, action_right;
		f32 start_x, start_y;
		f32 end_x, end_y;
		f32 average_x, average_y;
		f32 min_x, min_y;
		f32 max_x, max_y;
		bool is_connected, is_analog;
	};

	struct Mouse {
		Controller_Button left_button;
		Controller_Button right_button;
		i32 x, y;
	};

	struct Input {
		Controller controllers[2];
		Mouse mouse;
		f32 frame_dt;
	};

	struct Sound_Sample {
		i16 left, right;
	};

	struct Sound {
		slice1<Sound_Sample> samples;
		i32 samples_per_second;
	};

	using Screen = slice2<u32>;

	struct Memory {
		bool is_initialized;
		slice1<u8> permanent;
		slice1<u8> transient;
    	Platform::Read_File_Sync* read_file_sync;
    	Platform::Write_File_Sync* write_file_sync;
    	Platform::Free_File_Memory* free_file_memory;
	};

	struct Color {
		f32 red, green, blue;
	};

	struct World {
		Tiles::Map tile_map;
	};

	struct Game_State {
		World world;
		Arena world_arena;
		Tiles::Position player_pos;
		f32 pixels_per_unit;
		f32 sound_t_sin;
	};

	extern "C" void update_and_render(const Input& input, Memory& memory, Screen& screen_buffer);
	using Update_And_Render = decltype(update_and_render);
	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(Memory& memory, Sound& sound_buffer);
	using Get_Sound_Samples = decltype(get_sound_samples);

	static void draw_rectangle(Screen& screen, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32);
	static f32 get_pixels_per_unit(const Screen& screen);
	
	static void init_memory(Memory& memory);
	static Game_State& get_game_state(Memory& memory);
	static u32 get_hex_color(const Color& color);
}