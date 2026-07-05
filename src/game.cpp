#include "game.hpp"
#include "tiles.cpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen& screen) {
		assert(input.frame_dt > 0);
		if (!memory.is_initialized) init_memory(memory);

		auto& game_state = get_game_state(memory);
		auto& player_pos = game_state.player_pos;
		auto& tile_map = game_state.world.tile_map;

		auto new_player_pos = player_pos;
		for (auto& controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			f32 player_speed = controller.action_down.is_pressed ? 20.0f : 5.0f;

			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy =   player_speed;
			if (controller.move_down.is_pressed)  player_dy = - player_speed;
			
			new_player_pos.tile_rel_x += player_dx * input.frame_dt;
			new_player_pos.tile_rel_y += player_dy * input.frame_dt;
			Tiles::normalize_position(new_player_pos);
		}

		f32 player_width  = 1.0f;
		f32 player_height = 1.4f;

		auto new_player_pos_left = new_player_pos;
		new_player_pos_left.tile_rel_x -= player_width / 2;
		Tiles::normalize_position(new_player_pos_left);

		auto new_player_pos_right = new_player_pos;
		new_player_pos_right.tile_rel_x += player_width / 2;
		Tiles::normalize_position(new_player_pos_right);

		if (Tiles::check_empty_tile(tile_map, new_player_pos_left.abs_x,  new_player_pos_left.abs_y)
		 && Tiles::check_empty_tile(tile_map, new_player_pos.abs_x,       new_player_pos.abs_y)
		 && Tiles::check_empty_tile(tile_map, new_player_pos_right.abs_x, new_player_pos_right.abs_y)) {
			player_pos = new_player_pos;
		}

		draw_rectangle(screen, Color{ 1.0f, 0.0f, 1.0f }, 0.0f,
			SCENES_PER_SCREEN * SCENE_WIDTH_TILES  * Tiles::TILE_DIM,
			SCENES_PER_SCREEN * SCENE_HEIGHT_TILES * Tiles::TILE_DIM, 0.0f
		);

		i32 player_abs_x = cast_ignore_sign<i32>(player_pos.abs_x);
		i32 player_abs_y = cast_ignore_sign<i32>(player_pos.abs_y);
		i32 half_screen_width_tiles  = SCENE_WIDTH_TILES  * SCENES_PER_SCREEN / 2;
		i32 half_screen_height_tiles = SCENE_HEIGHT_TILES * SCENES_PER_SCREEN / 2;

		for (    i32 y = player_abs_y - half_screen_height_tiles - 1; y <= player_abs_y + half_screen_height_tiles + 1; ++y) {
			for (i32 x = player_abs_x - half_screen_width_tiles  - 1; x <= player_abs_x + half_screen_width_tiles  + 1; ++x) {
				auto tile = Tiles::get_tile(tile_map, cast_ignore_sign<u32>(x), cast_ignore_sign<u32>(y));

				Color color = {};
				switch (tile) {
					case Tiles::Tile::Not_Initialized: color = { 1.0f, 0.0f, 0.0f }; break;
					case Tiles::Tile::Walkable:        color = { 0.5f, 0.5f, 0.5f }; break;
					case Tiles::Tile::Wall:            color = { 1.0f, 1.0f, 1.0f }; break;
				}

				if (x == player_abs_x && y == player_abs_y) {
					color = Color{ 0.0f, 0.0f, 0.0f };
				}

				f32 min_x =   (x - player_abs_x + half_screen_width_tiles)  * Tiles::TILE_DIM - player_pos.tile_rel_x;
				f32 min_y = - (y - player_abs_y - half_screen_height_tiles) * Tiles::TILE_DIM + player_pos.tile_rel_y;
				f32 max_x = min_x + Tiles::TILE_DIM;
				f32 max_y = min_y - Tiles::TILE_DIM;
				draw_rectangle(screen, color, min_x, max_x, min_y, max_y);
			}
		}

		f32 player_min_x = half_screen_width_tiles  * Tiles::TILE_DIM - player_width / 2;
		f32 player_min_y = half_screen_height_tiles * Tiles::TILE_DIM;
		f32 player_max_x = player_min_x + player_width;
		f32 player_max_y = player_min_y - player_height;
		draw_rectangle(screen, Color{ 1.0f, 1.0f, 0.0f }, player_min_x, player_max_x, player_min_y, player_max_y);
	};

	extern "C" void get_sound_samples(Memory& memory, Sound& sound) {
		auto& game_state = get_game_state(memory);
		auto& sound_t_sin = game_state.sound_t_sin;

		// f32 volume = 5000.0f;
		f32 volume = 0;
		u32 frequency = 261;
		f32 samples_per_wave_period = cast<f32>(sound.samples_per_second / frequency);

		for (auto& sample : sound.samples) {
			i16 value = cast<i16>(std::sinf(sound_t_sin) * volume);
			sample.left  = value;
			sample.right = value;
			sound_t_sin += DOUBLE_PI32 / samples_per_wave_period;
			if (sound_t_sin >= DOUBLE_PI32) sound_t_sin -= DOUBLE_PI32;
		}
	}

	static void draw_rectangle(Screen& screen, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32) {
		hm::swap(min_y_f32, max_y_f32); // screen.base инвертирован по вертикали относительно координат игры

		f32 pixels_per_unit = get_pixels_per_unit(screen);
		// f32 offset_x = - Tiles::TILE_DIM / 2;
		f32 offset_x = 0;
		f32 offset_y = 0;

		i32 min_x = hm::round((min_x_f32 + offset_x) * pixels_per_unit);
		i32 max_x = hm::round((max_x_f32 + offset_x) * pixels_per_unit);
		i32 min_y = hm::round((min_y_f32 + offset_y) * pixels_per_unit);
		i32 max_y = hm::round((max_y_f32 + offset_y) * pixels_per_unit);

		min_x = hm::max(min_x, 0);
		max_x = hm::min(max_x, screen.count_x);
		min_y = hm::max(min_y, 0);
		max_y = hm::min(max_y, screen.count_y);

		u32 hex_color = get_hex_color(color);
		u32* row = screen.base + min_y * screen.count_x + min_x;
		for (i32 y = min_y; y < max_y; ++y) {
			u32* pixel = row;
			for (i32 x = min_x; x < max_x; ++x) {
				*pixel++ = hex_color;
			}
			row += screen.count_x;
		}
	};

	static void init_memory(Memory& memory) {
		auto& game_state = get_game_state(memory);
		auto& player_pos = game_state.player_pos;
		auto& tile_map = game_state.world.tile_map;
		auto& chunks = game_state.world.tile_map.chunks;
		auto& world_arena = game_state.world_arena;

		world_arena.base = memory.permanent.base + size_of(Game_State);
		world_arena.size = memory.permanent.get_size() - size_of(Game_State);

		chunks.count_x = Tiles::WORLD_DIM_CHUNKS;
		chunks.count_y = Tiles::WORLD_DIM_CHUNKS;
		chunks.base = world_arena.push<Tiles::Chunk>(chunks.get_size());

		u32 scene_x = 0, scene_y = 0;
		bool is_door_left = false, is_door_right = false, is_door_top = false, is_door_bottom = false;
		for (i32 scene_index = 0; scene_index < 100; ++scene_index) {
			bool is_next_scene_top = RANDOM_NUMBERS_TABLE[scene_index] % 2;
			if (is_next_scene_top) {
				is_door_top = true;
			} else {
				is_door_right = true;
			}

			for (    u32 tile_y = 0; tile_y < SCENE_HEIGHT_TILES; ++tile_y) {
				for (u32 tile_x = 0; tile_x < SCENE_WIDTH_TILES;  ++tile_x) {
					u32 abs_x = scene_x * SCENE_WIDTH_TILES  + tile_x;
					u32 abs_y = scene_y * SCENE_HEIGHT_TILES + tile_y;

					auto tile_value = Tiles::Tile::Walkable;
					if (tile_x == 0 || tile_x == SCENE_WIDTH_TILES  - 1 
					||  tile_y == 0 || tile_y == SCENE_HEIGHT_TILES - 1) {
						tile_value = Tiles::Tile::Wall;
					}

					if ((is_door_left   && tile_x == 0                     && tile_y == SCENE_HEIGHT_TILES / 2)
				     || (is_door_right  && tile_x == SCENE_WIDTH_TILES - 1 && tile_y == SCENE_HEIGHT_TILES / 2)
					 || (is_door_top    && tile_x == SCENE_WIDTH_TILES / 2 && tile_y == SCENE_HEIGHT_TILES - 1)
					 || (is_door_bottom && tile_x == SCENE_WIDTH_TILES / 2 && tile_y == 0                     )) {
						tile_value = Tiles::Tile::Walkable;
					}
					
					Tiles::set_tile(world_arena, tile_map, abs_x, abs_y, tile_value);
				}
			}

			if (is_next_scene_top) {
				scene_y += 1;
			} else {
				scene_x += 1;
			}

			is_door_left = is_door_right;
			is_door_bottom = is_door_top;
			is_door_right = false;
			is_door_top = false;
		}

		player_pos.abs_x = 1;
		player_pos.abs_y = 1;
		player_pos.tile_rel_x = Tiles::TILE_DIM / 2;
		player_pos.tile_rel_y = Tiles::TILE_DIM / 2;
		Tiles::normalize_position(player_pos);
		
		assert(size_of(Game_State) <= memory.permanent.get_size());
		assert(Tiles::check_empty_tile(tile_map, player_pos.abs_x, player_pos.abs_y));
		memory.is_initialized = true;
	}

	static u32 get_hex_color(const Color& color) {
		return (cast<u32>(hm::round(color.red   * 255.0f)) << 16)
			 | (cast<u32>(hm::round(color.green * 255.0f)) << 8)
			 | (cast<u32>(hm::round(color.blue  * 255.0f)));
	}

	static f32 get_pixels_per_unit(const Screen& screen) {
		return cast<f32>(screen.count_y) / (SCENES_PER_SCREEN * SCENE_HEIGHT_TILES * Tiles::TILE_DIM);
	}

	static Game_State& get_game_state(Memory& memory) {
		return *cast<Game_State*>(memory.permanent.base);
	}
}