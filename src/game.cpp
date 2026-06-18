#include "game.hpp"
#include "tiles.cpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen& screen) {
		if (!memory.is_initialized) init_memory(memory);

		auto& game_state = *(Game_State*)memory.permanent.base;
		auto& player_pos = game_state.player_pos;
		auto& tile_map = game_state.world.tile_map;

		auto new_player_pos = player_pos;
		for (auto& controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			f32 player_speed = controller.action_down.is_pressed ? 10.0f : 5.0f;

			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy =   player_speed;
			if (controller.move_down.is_pressed)  player_dy = - player_speed;
			
			new_player_pos.tile_x += player_dx * input.frame_dt;
			new_player_pos.tile_y += player_dy * input.frame_dt;
			Tiles::normalize_position(new_player_pos);
		}

		f32 player_width  = 1.0f;
		f32 player_height = 1.4f;

		auto new_player_pos_left = new_player_pos;
		new_player_pos_left.tile_x -= player_width / 2;
		Tiles::normalize_position(new_player_pos_left);

		auto new_player_pos_right = new_player_pos;
		new_player_pos_right.tile_x += player_width / 2;
		Tiles::normalize_position(new_player_pos_right);

		if (Tiles::check_empty_tile(tile_map, new_player_pos_left)
		 && Tiles::check_empty_tile(tile_map, new_player_pos)
		 && Tiles::check_empty_tile(tile_map, new_player_pos_right)) {
			player_pos = new_player_pos;
		}

		// TODO: player_chunk_pos это остатки старой реализации, пока оставил чтобы закоммитить проект в относительно рабочем состоянии.
		// Сейчас при движении по вертикали видно как появляются/исчезают стенки, видимо сейчас мы рисуем стенки только для текущего чанка
		auto player_chunk_pos = Tiles::get_chunk_position(player_pos.world_x, player_pos.world_y);
		
		i32 half_screen_width_tiles  = SCREEN_WIDTH_TILES  / 2;
		i32 half_screen_height_tiles = SCREEN_HEIGHT_TILES / 2;

		draw_rectangle(screen, Color{ 1.0f, 0.0f, 1.0f }, 0.0f, SCREEN_WIDTH_TILES * Tiles::TILE_SIZE, SCREEN_HEIGHT_TILES * Tiles::TILE_SIZE, 0.0f);
		for (    i32 y = player_chunk_pos.chunk_y - half_screen_height_tiles - 1; y <= player_chunk_pos.chunk_y + half_screen_height_tiles + 1; ++y) {
			for (i32 x = player_chunk_pos.chunk_x - half_screen_width_tiles  - 1; x <= player_chunk_pos.chunk_x + half_screen_width_tiles  + 1; ++x) {

				Color color = y >= 0 && x >= 0 && Tiles::get_tile(tile_map, static_cast<u32>(x), static_cast<u32>(y))
					? Color{ 1.0f, 1.0f, 1.0f }
					: Color{ 0.5f, 0.5f, 0.5f };

				if (x == player_chunk_pos.chunk_x && y == player_chunk_pos.chunk_y) {
					color = Color{ 0.0f, 0.0f, 0.0f };
				}

				f32 min_x =   (x - player_chunk_pos.chunk_x + half_screen_width_tiles)  * Tiles::TILE_SIZE - player_pos.tile_x;
				f32 min_y = - (y - player_chunk_pos.chunk_y - half_screen_height_tiles) * Tiles::TILE_SIZE + player_pos.tile_y;
				f32 max_x = min_x + Tiles::TILE_SIZE;
				f32 max_y = min_y - Tiles::TILE_SIZE;
				draw_rectangle(screen, color, min_x, max_x, min_y, max_y);
			}
		}

		f32 player_min_x = half_screen_width_tiles  * Tiles::TILE_SIZE - player_width / 2;
		f32 player_min_y = half_screen_height_tiles * Tiles::TILE_SIZE;
		f32 player_max_x = player_min_x + player_width;
		f32 player_max_y = player_min_y - player_height;
		draw_rectangle(screen, Color{ 1.0f, 0.0f, 0.0f }, player_min_x, player_max_x, player_min_y, player_max_y);
	};

	extern "C" void get_sound_samples(Memory& memory, Sound& sound) {
		for (auto& sample : sound.samples) {
			sample = {};
		}
	}

	static void draw_rectangle(Screen& screen, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32) {
		hm::swap(min_y_f32, max_y_f32); // screen.pixels инвертирован по вертикали относительно координат игры

		f32 pixels_per_unit = get_pixels_per_unit(screen);
		f32 offset_x = - Tiles::TILE_SIZE / 2;
		f32 offset_y = 0;

		i32 min_x = hm::round((min_x_f32 + offset_x) * pixels_per_unit);
		i32 max_x = hm::round((max_x_f32 + offset_x) * pixels_per_unit);
		i32 min_y = hm::round((min_y_f32 + offset_y) * pixels_per_unit);
		i32 max_y = hm::round((max_y_f32 + offset_y) * pixels_per_unit);

		min_x = hm::max(min_x, 0);
		max_x = hm::min(max_x, screen.width);
		min_y = hm::max(min_y, 0);
		max_y = hm::min(max_y, screen.height);

		u32 hex_color = get_hex_color(color);
		u32* row = screen.pixels + min_y * screen.width + min_x;
		for (i32 y = min_y; y < max_y; ++y) {
			u32* pixel = row;
			for (i32 x = min_x; x < max_x; ++x) {
				*pixel++ = hex_color;
			}
			row += screen.width;
		}
	};

	static void init_memory(Memory& memory) {
		auto& game_state = *(Game_State*)memory.permanent.base;
		auto& player_pos = game_state.player_pos;
		auto& tile_map = game_state.world.tile_map;
		auto& chunks = game_state.world.tile_map.chunks;
		auto& world_arena = game_state.world_arena;

		world_arena.base = memory.permanent.base + size_of(Game_State);
		world_arena.size = memory.permanent.size - size_of(Game_State);

		chunks.size = size_of(Tiles::Chunk) * Tiles::WORLD_SIZE_CHUNKS * Tiles::WORLD_SIZE_CHUNKS;
		chunks.base = (Tiles::Chunk*)arena_push<u8>(world_arena, chunks.size).base;

		for (    i32 chunk_y = 0; chunk_y < Tiles::WORLD_SIZE_CHUNKS; ++chunk_y) {
			for (i32 chunk_x = 0; chunk_x < Tiles::WORLD_SIZE_CHUNKS; ++chunk_x) {
				auto& chunk = chunks[chunk_y * Tiles::WORLD_SIZE_CHUNKS + chunk_x];
				chunk.tiles.size = size_of(Tiles::Tile) * Tiles::CHUNK_SIZE_TILES * Tiles::CHUNK_SIZE_TILES;
				chunk.tiles.base = (Tiles::Tile*)arena_push<u8>(world_arena, chunk.tiles.size).base;
			}
		}
		
		for (    u32 screen_y = 0; screen_y < 32; ++screen_y) {
			for (u32 screen_x = 0; screen_x < 32; ++screen_x) {
				for (    u32 tile_y = 0; tile_y < SCREEN_HEIGHT_TILES; ++tile_y) {
					for (u32 tile_x = 0; tile_x < SCREEN_WIDTH_TILES;  ++tile_x) {
						u32 tile_abs_x = screen_x * SCREEN_WIDTH_TILES  + tile_x;
						u32 tile_abs_y = screen_y * SCREEN_HEIGHT_TILES + tile_y;
						Tiles::set_tile(tile_map, tile_abs_x, tile_abs_y, (tile_x == tile_y) && tile_y % 2);
					}
				}
			}
		}

		player_pos.world_x = 1;
		player_pos.world_y = 1;
		player_pos.tile_x = 1.0f;
		player_pos.tile_y = 1.0f;
		Tiles::normalize_position(player_pos);
		
		assert(size_of(Game_State) <= memory.permanent.size);
		assert(Tiles::check_empty_tile(tile_map, player_pos));
		memory.is_initialized = true;
	}

	static u32 get_hex_color(const Color& color) {
		return ((u32)hm::round(color.red   * 255.0f) << 16)
			 | ((u32)hm::round(color.green * 255.0f) << 8)
			 | ((u32)hm::round(color.blue  * 255.0f));
	}

	static f32 get_pixels_per_unit(const Screen& screen) {
		return (f32)screen.height / (SCREEN_HEIGHT_TILES * Tiles::TILE_SIZE);
	}
}