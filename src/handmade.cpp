#include "handmade.hpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer) {
		assert((i64)sizeof(Game_State) <= memory.permanent_size);
		Game_State& game_state = *(Game_State*)memory.permanent_storage;
		if (!memory.is_initialized) {
			memory.is_initialized = true;
			game_state.player_x = 60.0f;
			game_state.player_y = 60.0f;
		}

		const i32 TILES_COUNT_Y = 9;
		const i32 TILES_COUNT_X = 17;

		i32 tiles_00[TILES_COUNT_Y][TILES_COUNT_X] = {
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0 },
			{ 1,0,0,0,0,1, 0,0,0,0,0, 1,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
		};

		i32 tiles_01[TILES_COUNT_Y][TILES_COUNT_X] = {
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 0,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 0,0,0,0,0, 1,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
		};
		

		i32 tiles_10[TILES_COUNT_Y][TILES_COUNT_X] = {
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0 },
			{ 1,0,0,0,0,1, 0,0,0,0,0, 1,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
		};

		i32 tiles_11[TILES_COUNT_Y][TILES_COUNT_X] = {
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 0,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 0,0,0,0,0, 1,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
		};

		Tile_Map tile_maps[2][2] = {};
		tile_maps[0][0] = {};
		tile_maps[0][0].start_x = - 30.0f;
		tile_maps[0][0].start_y = 0;
		tile_maps[0][0].count_x = TILES_COUNT_X;
		tile_maps[0][0].count_y = TILES_COUNT_Y;
		tile_maps[0][0].tile_width  = 60.0f;
		tile_maps[0][0].tile_height = 60.0f;
		tile_maps[0][0].tiles = *tiles_00;

		// TODO: start_x, start_y?
		tile_maps[0][1] = tile_maps[0][0];
		tile_maps[0][1].tiles = *tiles_01;
		tile_maps[1][0] = tile_maps[0][0];
		tile_maps[1][0].tiles = *tiles_10;
		tile_maps[1][1] = tile_maps[0][0];
		tile_maps[1][1].tiles = *tiles_11;

		Tile_Map& tile_map = tile_maps[0][0];
		// World_Map& world_map = {};
		// world_map.count_x = 2;
		// world_map.count_y = 2;
		// world_map.tile_maps = tile_maps[0];

		f32 player_width  = tile_map.tile_width * 0.75f;
		f32 player_height = tile_map.tile_height;
		f32 player_speed  = 200.0f;

		f32 new_player_x = game_state.player_x;
		f32 new_player_y = game_state.player_y;
		for (auto& controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy = - player_speed;
			if (controller.move_down.is_pressed)  player_dy =   player_speed;
			new_player_x += player_dx * input.frame_dt;
			new_player_y += player_dy * input.frame_dt;
		}

		if (check_tile_point_empty(tile_map, new_player_x - 0.5f * player_width, new_player_y) &&
	    	check_tile_point_empty(tile_map, new_player_x + 0.5f * player_width, new_player_y)) {
			game_state.player_x = new_player_x;
			game_state.player_y = new_player_y;
		}
		
		draw_rectangle(screen_buffer, Color{ 1.0f, 0.0f, 1.0f }, 0.0f, (f32)screen_buffer.width, 0.0f, (f32)screen_buffer.height);
		for (i32 tile_y = 0; tile_y < tile_map.count_y; tile_y++) {
			for (i32 tile_x = 0; tile_x < tile_map.count_x; tile_x++) {
				f32 min_x = tile_x * tile_map.tile_width  + tile_map.start_x;
				f32 min_y = tile_y * tile_map.tile_height + tile_map.start_y;
				f32 max_x = min_x + tile_map.tile_width;
				f32 max_y = min_y + tile_map.tile_height;
				Color color = tile_map.tiles[tile_y * tile_map.count_x + tile_x] ? Color{ 1.0f, 1.0f, 1.0f } : Color{ 0.5f, 0.5f, 0.5f };
				draw_rectangle(screen_buffer, color, min_x, max_x, min_y, max_y);
			}
		}

		f32 player_min_x = game_state.player_x - 0.5f * player_width;
		f32 player_min_y = game_state.player_y - player_height;
		f32 player_max_x = player_min_x + player_width;
		f32 player_max_y = player_min_y + player_height;
		draw_rectangle(screen_buffer, Color{ 1.0f, 0.0f, 0.0f }, player_min_x, player_max_x, player_min_y, player_max_y);
	};

	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer) {
		Sound_Sample* sample = sound_buffer.samples;
		for (i32 i = 0; i < sound_buffer.samples_to_write; i++) {
			sample->left  = 0;
			sample->right = 0;
			sample++;
		}
	}

	static bool check_world_point_empty(const World_Map& world_map, i32 world_x, i32 world_y, f32 tile_test_x, f32 tile_test_y) {
		Tile_Map* tile_map = get_tile_map(world_map, world_x, world_y);
		if (!tile_map) return false;
		return check_tile_point_empty(*tile_map, tile_test_x, tile_test_y);
	}

	// TODO: заинлайнить в check_world_point_empty?
	static bool check_tile_point_empty(const Tile_Map& tile_map, f32 tile_test_x, f32 tile_test_y) {
		i32 tile_x = (i32)((tile_test_x - tile_map.start_x) / tile_map.tile_width);
		i32 tile_y = (i32)((tile_test_y - tile_map.start_y) / tile_map.tile_height);
		return tile_y >= 0 && tile_y < tile_map.count_y &&
	           tile_x >= 0 && tile_x < tile_map.count_x &&
		       tile_map.tiles[tile_y * tile_map.count_x + tile_x] == 0;
	}

	// TODO: заинлайнить в check_world_point_empty?
	static Tile_Map* get_tile_map(const World_Map& world_map, i32 world_x, i32 world_y) {
		if (world_y >= 0 && world_y < world_map.count_y &&
			world_x >= 0 && world_x < world_map.count_x) {
			return &world_map.tile_maps[world_y * world_map.count_x + world_x];
		}
		return nullptr;
	}

	static void draw_rectangle(Screen_Buffer& screen_buffer, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32) {
		i32 min_x = (i32)hm::round(min_x_f32);
		i32 min_y = (i32)hm::round(min_y_f32);
		i32 max_x = (i32)hm::round(max_x_f32);
		i32 max_y = (i32)hm::round(max_y_f32);

		min_x = hm::max(min_x, 0);
		min_y = hm::max(min_y, 0);
		max_x = hm::min(max_x, screen_buffer.width);
		max_y = hm::min(max_y, screen_buffer.height);

		u32 hex_color = color.to_hex();
		u32* row = screen_buffer.pixels + min_y * screen_buffer.width + min_x;
		for (i32 y = min_y; y < max_y; y++) {
			u32* pixel = row;
			for (i32 x = min_x; x < max_x; x++) {
				*pixel++ = hex_color;
			}
			row += screen_buffer.width;
		}
	};

	static void dev_render_mouse_test(const Input& input, Screen_Buffer& screen_buffer) {
		if constexpr (!DEV_MODE) return;
		draw_rectangle(screen_buffer, Color{ 1.0f, 1.0f, 1.0f }, (f32)input.dev_mouse.x, (f32)input.dev_mouse.x + 10.0f, (f32)input.dev_mouse.y, (f32)input.dev_mouse.y + 10.0f);
		if (input.dev_mouse.left_button.is_pressed) draw_rectangle(screen_buffer, Color{ 1.0f, 1.0f, 1.0f }, 10.0f, 20.0f, 10.0f, 20.0f);
		if (input.dev_mouse.right_button.is_pressed) draw_rectangle(screen_buffer, Color{ 1.0f, 1.0f, 1.0f }, 30.0f, 40.0f, 10.0f, 20.0f);
	};
}