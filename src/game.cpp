#include "game.hpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen& screen) {
		if (!memory.is_initialized) init_memory(memory);
		Game_State& game_state = *(Game_State*)memory.permanent_storage.ptr;

		f32 player_height = 1.4f;
		f32 player_width  = 1.0f;

		World_Position new_player_pos = game_state.player_pos;
		for (auto& controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			f32 player_speed = controller.action_down.is_pressed ? 20.0f : 5.0f;

			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy =   player_speed;
			if (controller.move_down.is_pressed)  player_dy = - player_speed;
			
			new_player_pos.tile_x += player_dx * input.frame_dt;
			new_player_pos.tile_y += player_dy * input.frame_dt;
			normalize_position(new_player_pos);
		}

		World_Position new_player_pos_left  = new_player_pos;
		new_player_pos_left.tile_x  -= player_width / 2;
		normalize_position(new_player_pos_left);

		World_Position new_player_pos_right = new_player_pos;
		new_player_pos_right.tile_x += player_width / 2;
		normalize_position(new_player_pos_right);

		if (check_empty_tile(game_state.world, new_player_pos_left)
		 && check_empty_tile(game_state.world, new_player_pos)
		 && check_empty_tile(game_state.world, new_player_pos_right)) {
			game_state.player_pos = new_player_pos;
		}

		Chunk& current_chunk = game_state.world.get_chunk(game_state.player_pos);
		Chunk_Position player_chunk_pos = get_chunk_position(game_state.player_pos);
		
		i32 half_screen_width_tiles  = SCREEN_WIDTH_TILES  / 2;
		i32 half_screen_height_tiles = SCREEN_HEIGHT_TILES / 2;

		draw_rectangle(screen, Color{ 1.0f, 0.0f, 1.0f }, 0.0f, SCREEN_WIDTH_TILES * TILE_SIZE, SCREEN_HEIGHT_TILES * TILE_SIZE, 0.0f);
		for (    i32 y = player_chunk_pos.chunk_y - half_screen_height_tiles - 1; y <= player_chunk_pos.chunk_y + half_screen_height_tiles + 1; y++) {
			for (i32 x = player_chunk_pos.chunk_x - half_screen_width_tiles  - 1; x <= player_chunk_pos.chunk_x + half_screen_width_tiles  + 1;  x++) {

				Color color = y >= 0 && x >= 0 && current_chunk.tiles[y][x] ? Color{ 1.0f, 1.0f, 1.0f } : Color{ 0.5f, 0.5f, 0.5f };
				if (x == player_chunk_pos.chunk_x && y == player_chunk_pos.chunk_y) color = Color{ 0.0f, 0.0f, 0.0f };

				f32 min_x =   (x - player_chunk_pos.chunk_x + half_screen_width_tiles)  * TILE_SIZE - player_chunk_pos.tile_x;
				f32 min_y = - (y - player_chunk_pos.chunk_y - half_screen_height_tiles) * TILE_SIZE + player_chunk_pos.tile_y;
				f32 max_x = min_x + TILE_SIZE;
				f32 max_y = min_y - TILE_SIZE;
				draw_rectangle(screen, color, min_x, max_x, min_y, max_y);
			}
		}

		f32 player_min_x = half_screen_width_tiles  * TILE_SIZE - player_width / 2;
		f32 player_min_y = half_screen_height_tiles * TILE_SIZE;
		f32 player_max_x = player_min_x + player_width;
		f32 player_max_y = player_min_y - player_height;
		draw_rectangle(screen, Color{ 1.0f, 0.0f, 0.0f }, player_min_x, player_max_x, player_min_y, player_max_y);
	};

	extern "C" void get_sound_samples(Memory& memory, Sound& sound) {
		Sound_Sample* sample = sound.samples;
		for (i32 i = 0; i < sound.samples_to_write; i++) {
			*sample = {};
			sample++;
		}
	}

	static bool check_empty_tile(const World& world, const World_Position& position) {
		return world.get_chunk(position).tiles[position.world_y][position.world_x] == 0;
	}

	// TODO: учесть возможность смещения больше чем на 1 клетку
	static void normalize_position(World_Position& position) {
		if (position.tile_x < 0) {
			position.world_x  -= 1;
			position.tile_x += TILE_SIZE;
		} else if (position.tile_x >= TILE_SIZE) {
			position.world_x  += 1;
			position.tile_x -= TILE_SIZE;
		}

		if (position.tile_y < 0) {
			position.world_y  -= 1;
			position.tile_y += TILE_SIZE;
		} else if (position.tile_y >= TILE_SIZE) {
			position.world_y  += 1;
			position.tile_y -= TILE_SIZE;
		}

		// TODO: при получении дробной части через деление можно получить ненормализованное значение из-за округления
		// можно на время сделать <= TILE_SIZE
		assert(position.tile_x >= 0 && position.tile_x < TILE_SIZE);
		assert(position.tile_y >= 0 && position.tile_y < TILE_SIZE);
	}

	static Chunk_Position get_chunk_position(const World_Position& world_pos) {
		Chunk_Position result = {};
		result.world_x = world_pos.world_x >> CHUNK_POSITION_SHIFT;
		result.world_y = world_pos.world_y >> CHUNK_POSITION_SHIFT;
		result.chunk_x = (i32)(world_pos.world_x & CHUNK_POSITION_MASK);
		result.chunk_y = (i32)(world_pos.world_y & CHUNK_POSITION_MASK);
		result.tile_x = world_pos.tile_x;
		result.tile_y = world_pos.tile_y;
		return result;
	}

	static void draw_rectangle(Screen& screen, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32) {
		// screen.pixels инвертирован по вертикали относительно координат игры
		hm::swap(min_y_f32, max_y_f32);

		f32 pixels_per_unit = screen.get_pixels_per_unit();
		f32 offset_x = - TILE_SIZE / 2;
		f32 offset_y = 0;

		i32 min_x = hm::round((min_x_f32 + offset_x) * pixels_per_unit);
		i32 max_x = hm::round((max_x_f32 + offset_x) * pixels_per_unit);
		i32 min_y = hm::round((min_y_f32 + offset_y) * pixels_per_unit);
		i32 max_y = hm::round((max_y_f32 + offset_y) * pixels_per_unit);

		min_x = hm::max(min_x, 0);
		max_x = hm::min(max_x, screen.width);
		min_y = hm::max(min_y, 0);
		max_y = hm::min(max_y, screen.height);

		u32 hex_color = color.to_hex();
		u32* row = screen.pixels + min_y * screen.width + min_x;
		for (i32 y = min_y; y < max_y; y++) {
			u32* pixel = row;
			for (i32 x = min_x; x < max_x; x++) {
				*pixel++ = hex_color;
			}
			row += screen.width;
		}
	};

	static void init_memory(Memory& memory) {
		Game_State& game_state = *(Game_State*)memory.permanent_storage.ptr;

		i32 TILES[CHUNK_SIZE_TILES][CHUNK_SIZE_TILES] = {
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1, 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1, 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 1,0,0,0,0, 1,0,0,0,0,1, 1,0,0,0,0,1, 0,0,0,0,1, 0,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1, 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1, 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1, 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1, 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1, 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },

			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1, 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1, 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1, 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,0, 1,0,0,0,0, 1,0,0,0,0,1, 1,0,0,0,0,1, 0,0,0,0,1, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1, 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1, 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1, 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1, 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
		};

		hm::memcpy(TILES, game_state.TILES);

		Chunk CHUNKS[WORLD_SIZE_CHUNKS][WORLD_SIZE_CHUNKS] = {
			{ Chunk{ game_state.TILES }, Chunk{ game_state.TILES } },
			{ Chunk{ game_state.TILES }, Chunk{ game_state.TILES } },
		};
		
		hm::memcpy(CHUNKS, game_state.CHUNKS);
		game_state.world.chunks = (Chunk*)game_state.CHUNKS;

		game_state.player_pos.world_x = 1;
		game_state.player_pos.world_y = 1;
		game_state.player_pos.tile_x = 1.0f;
		game_state.player_pos.tile_y = 1.0f;
		normalize_position(game_state.player_pos);
		
		assert((i64)sizeof(Game_State) <= memory.permanent_storage.size);
		assert(check_empty_tile(game_state.world, game_state.player_pos));
		memory.is_initialized = true;
	}
}