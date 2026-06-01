#pragma once

#include "globals.hpp"

namespace Game {
	static const i32 WORLD_SIZE_CHUNKS = 2;
	static const i32 CHUNK_POSITION_SHIFT = 8;
	static const i32 CHUNK_SIZE_TILES = 1 << CHUNK_POSITION_SHIFT;
	static const u32 CHUNK_POSITION_MASK = CHUNK_SIZE_TILES - 1;
	static const f32 TILE_SIZE = 1.4f;
	static const i32 SCREEN_WIDTH_TILES = 17;
	static const i32 SCREEN_HEIGHT_TILES = 9;

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

	struct Mouse {
		i32 x, y;
		Button left_button;
		Button right_button;
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
		i32 samples_per_second;
		i32 samples_to_write;
		Sound_Sample* samples;
	};

	struct Screen {
		i32 width, height;
		u32* pixels;
	};

	struct Memory {
		bool is_initialized;
		span<u8> permanent_storage;
		span<u8> transient_storage;
    	Platform::Read_File_Sync* read_file_sync;
    	Platform::Write_File_Sync* write_file_sync;
    	Platform::Free_File_Memory* free_file_memory;
	};

	struct Color {
		f32 red, green, blue;
	};

	struct Chunk {
		span<i32> tiles;
	};

	struct World {
		span<Chunk> chunks;
	};

	// TODO: сделать сеттеры с автоматической нормализацией после введения векторов + конструктор с нормализацией
	struct World_Position {
		u32 world_x, world_y;
		f32 tile_x, tile_y;
	};

	struct Chunk_Position {
		u32 world_x, world_y;
		i32 chunk_x, chunk_y;
		f32 tile_x, tile_y;
	};

	struct Game_State {
		i32 TILES[CHUNK_SIZE_TILES][CHUNK_SIZE_TILES];
		Chunk CHUNKS[WORLD_SIZE_CHUNKS][WORLD_SIZE_CHUNKS];
		World world;
		World_Position player_pos;
		f32 pixels_per_unit;
	};

	extern "C" void update_and_render(const Input& input, Memory& memory, Screen& screen_buffer);
	using Update_And_Render = decltype(update_and_render);
	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(Memory& memory, Sound& sound_buffer);
	using Get_Sound_Samples = decltype(get_sound_samples);

	// TODO: появляются первые признаки const-poisoning, подумать над выпиливанием const из параметров функций
	static bool check_empty_tile(World& world, const World_Position& position);
	static void draw_rectangle(Screen& screen, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32);
	static i32& get_chunk_tile(Chunk& chunk, const Chunk_Position& position);
	static u32 get_hex_color(const Color& color);
	static f32 get_pixels_per_unit(const Screen& screen);
	static Chunk& get_world_chunk(World& world, const World_Position& position);
	static void init_memory(Memory& memory);
	static Chunk_Position get_chunk_position(const World_Position& world_pos);
	static void normalize_position(World_Position& position);
}