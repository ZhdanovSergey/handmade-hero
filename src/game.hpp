#pragma once

#include "globals.hpp"

namespace Game {
	struct Button {
		bool is_pressed;
		i32 transitions_count;
	};

	struct Controller {
		bool is_connected, is_analog;
		f32 start_x, start_y;
		f32 end_x, end_y;
		f32 average_x, average_y;
		f32 min_x, min_y;
		f32 max_x, max_y;
		Button start, back;
		Button left_shoulder, right_shoulder;
		Button move_up, move_down, move_left, move_right;
		Button action_up, action_down, action_left, action_right;
	};

	struct Dev_Mouse {
		i32 x, y;
		Button left_button;
		Button right_button;
	};

	struct Input {
		Controller controllers[2];
		Dev_Mouse dev_mouse;
		f32 frame_dt;
	};

	struct Color {
		f32 red, green, blue;
		u32 to_hex() const {
			return ((u32)hm::round(red   * 255.0f) << 16)
				 | ((u32)hm::round(green * 255.0f) << 8)
				 | ((u32)hm::round(blue  * 255.0f));
		}
	};

	struct Sound_Sample {
		i16 left, right;
	};

	struct Sound {
		i32 samples_per_second;
		i32 samples_to_write;
		Sound_Sample* samples;
	};

	struct Chunk_Position {
		u32 world_x, world_y;
		i32 chunk_x, chunk_y;
		f32 tile_x, tile_y;
	};

	// TODO: сделать сеттеры с автоматической нормализацией после введения векторов + конструктор с нормализацией
	struct World_Position {
		u32 world_x, world_y;
		f32 tile_x, tile_y;
	};

	struct Chunk {
		// TODO: переместить все игровые константы в основной блок памяти
		static constexpr i32 SHIFT = 8;
		static constexpr i32 SIZE = 1 << SHIFT;
		static constexpr u32 MASK = SIZE - 1;

		i32 (*tiles)[Chunk::SIZE];
	};

	struct World {
		static constexpr i32 WIDTH = 2;
		static constexpr i32 HEIGHT = 2;
		static constexpr f32 TILE_SIZE = 1.4f;

		Chunk* chunks;
		
		Chunk& get_chunk(const World_Position& position) const {
			// return chunks[player_pos.scene_y * World::WIDTH + player_pos.scene_x];
			return *chunks;
		};
	};

	struct Screen {
		static constexpr i32 WIDTH_TILES = 17;
		static constexpr i32 HEIGHT_TILES = 9;
		
		i32 width, height;
		u32* pixels;
		f32 get_pixels_per_unit() { return (f32)height / (HEIGHT_TILES * World::TILE_SIZE); }
	};

	struct Game_State {
		i32 TILES[Chunk::SIZE][Chunk::SIZE];
		Chunk CHUNKS[World::HEIGHT][World::WIDTH];
		World world;
		World_Position player_pos;
		f32 pixels_per_unit;
	};

	struct Memory {
		bool is_initialized;
		span<u8> permanent_storage;
		span<u8> transient_storage;
    	Platform::Read_File_Sync* read_file_sync;
    	Platform::Write_File_Sync* write_file_sync;
    	Platform::Free_File_Memory* free_file_memory;

		i64 get_total_size() const { return permanent_storage.size + transient_storage.size; }
	};

	extern "C" void update_and_render(const Input& input, Memory& memory, Screen& screen_buffer);
	using Update_And_Render = decltype(update_and_render);
	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(Memory& memory, Sound& sound_buffer);
	using Get_Sound_Samples = decltype(get_sound_samples);

	static bool check_empty_tile(const World& world, const World_Position& position);
	static void draw_rectangle(Screen& screen, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32);
	static Chunk_Position get_chunk_position(const World_Position& world_pos);
	static Game_State& get_initialized_game_state(Memory& memory);
	static void normalize_position(World_Position& position);
}