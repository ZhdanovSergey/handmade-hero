#include "game.hpp"

namespace Game {
	extern "C" void get_sound_samples(Memory& memory, Sound& sound) {
		Sound_Sample* sample = sound.samples;
		for (i32 i = 0; i < sound.samples_to_write; i++) {
			*sample = {};
			sample++;
		}
	}

	extern "C" void update_and_render(const Input& input, Memory& memory, Screen& screen) {
		Game_State& state = memory.get_game_state();
		f32 player_speed  = 5.0f;
		f32 player_height = 1.4f;
		f32 player_width  = 1.0f;

		World_Position new_player_pos = state.player_pos;
		for (auto& controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy =   player_speed;
			if (controller.move_down.is_pressed)  player_dy = - player_speed;
			new_player_pos.point_x += player_dx * input.frame_dt;
			new_player_pos.point_y += player_dy * input.frame_dt;
			new_player_pos.normalize();
		}

		World_Position new_player_pos_left  = new_player_pos;
		new_player_pos_left.point_x  -= player_width / 2;
		new_player_pos_left.normalize();

		World_Position new_player_pos_right = new_player_pos;
		new_player_pos_right.point_x += player_width / 2;
		new_player_pos_right.normalize();

		if (state.world.check_empty_tile(new_player_pos_left)
		 && state.world.check_empty_tile(new_player_pos)
		 && state.world.check_empty_tile(new_player_pos_right)) {
			state.player_pos = new_player_pos;
		}

		Chunk& current_chunk = state.world.get_chunk(state.player_pos);
		Chunk_Position player_chunk_pos = state.player_pos.get_chunk_position();

		screen.draw_rectangle(Color{ 1.0f, 0.0f, 1.0f }, 0.0f, Screen::WIDTH_TILES * World::TILE_SIZE, Screen::HEIGHT_TILES * World::TILE_SIZE, 0.0f);
		for (i32 y = 0; y < Screen::HEIGHT_TILES; y++) {
			for (i32 x = 0; x < Screen::WIDTH_TILES; x++) {
				Color color = current_chunk.tiles[y][x] ? Color{ 1.0f, 1.0f, 1.0f } : Color{ 0.5f, 0.5f, 0.5f };
				if (x == player_chunk_pos.chunk_x && y == player_chunk_pos.chunk_y) {
					color = Color{ 0.0f, 0.0f, 0.0f };
				}

				f32 min_x = x * World::TILE_SIZE;
				f32 max_x = min_x + World::TILE_SIZE;
				f32 min_y = (Screen::HEIGHT_TILES - y) * World::TILE_SIZE;
				f32 max_y = min_y - World::TILE_SIZE;
				screen.draw_rectangle(color, min_x, max_x, min_y, max_y);
			}
		}

		f32 player_min_x = player_chunk_pos.chunk_x * World::TILE_SIZE + player_chunk_pos.tile_x - player_width / 2;
		f32 player_max_x = player_min_x + player_width;
		f32 player_min_y = (Screen::HEIGHT_TILES - player_chunk_pos.chunk_y) * World::TILE_SIZE - player_chunk_pos.tile_y;
		f32 player_max_y = player_min_y - player_height;
		screen.draw_rectangle(Color{ 1.0f, 0.0f, 0.0f }, player_min_x, player_max_x, player_min_y, player_max_y);
	};

	bool World::check_empty_tile(const World_Position& position) {
		auto& world = *this;
		return world.get_chunk(position).tiles[position.tile_y][position.tile_x] == 0;
	}

	// TODO: учесть возможность смещения больше чем на 1 клетку
	void World_Position::normalize() {
		auto& position = *this;

		if (position.point_x < 0) {
			position.tile_x  -= 1;
			position.point_x += World::TILE_SIZE;
		} else if (position.point_x >= World::TILE_SIZE) {
			position.tile_x  += 1;
			position.point_x -= World::TILE_SIZE;
		}

		if (position.point_y < 0) {
			position.tile_y  -= 1;
			position.point_y += World::TILE_SIZE;
		} else if (position.point_y >= World::TILE_SIZE) {
			position.tile_y  += 1;
			position.point_y -= World::TILE_SIZE;
		}

		// TODO: при получении дробной части через деление можно получить ненормализованное значение из-за округления
		// можно на время сделать <= World::TILE_SIZE
		assert(position.point_x >= 0 && position.point_x < World::TILE_SIZE);
		assert(position.point_y >= 0 && position.point_y < World::TILE_SIZE);
	}

	Chunk_Position World_Position::get_chunk_position() {
		auto& world_pos = *this;
		Chunk_Position result = {};
		result.world_x = world_pos.tile_x >> Chunk::SHIFT;
		result.world_y = world_pos.tile_y >> Chunk::SHIFT;
		result.chunk_x = (i32)(world_pos.tile_x & Chunk::MASK);
		result.chunk_y = (i32)(world_pos.tile_y & Chunk::MASK);
		result.tile_x = world_pos.point_x;
		result.tile_y = world_pos.point_y;
		return result;
	}

	void Screen::draw_rectangle(const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32) {
		auto& screen = *this;
		// screen.pixels инвертирован по вертикали относительно координат игры
		hm::swap(min_y_f32, max_y_f32);

		f32 pixels_per_unit = screen.get_pixels_per_unit();
		f32 offset_x = - World::TILE_SIZE / 2;
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

	Game_State& Memory::get_game_state() {
		auto& memory = *this;
		Game_State& state = *(Game_State*)memory.permanent_storage.ptr;
		if (memory.is_initialized) return state;

		i32 TILES[Chunk::SIZE][Chunk::SIZE] = {
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

		hm::memcpy(TILES, state.TILES);

		Chunk CHUNKS[World::HEIGHT][World::WIDTH] = {
			{ Chunk{ state.TILES }, Chunk{ state.TILES } },
			{ Chunk{ state.TILES }, Chunk{ state.TILES } },
		};
		
		hm::memcpy(CHUNKS, state.CHUNKS);
		state.world.chunks = (Chunk*)state.CHUNKS;

		state.player_pos.tile_x = 1;
		state.player_pos.tile_y = 1;
		state.player_pos.point_x = 1.0f;
		state.player_pos.point_y = 1.0f;
		state.player_pos.normalize();
		
		assert((i64)sizeof(Game_State) <= memory.permanent_storage.size);
		assert(state.world.check_empty_tile(state.player_pos));
		
		memory.is_initialized = true;
		return state;
	}
}