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
		f32 player_speed  = 200.0f;
		f32 player_width  = state.world.tile_size_pixels * 0.75f;
		f32 player_height = state.world.tile_size_pixels;

		Position new_player_pos = state.player_pos;
		for (auto& controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy = - player_speed;
			if (controller.move_down.is_pressed)  player_dy =   player_speed;
			new_player_pos.point_x += player_dx * input.frame_dt;
			new_player_pos.point_y += player_dy * input.frame_dt;
			new_player_pos.normalize(state.world.tile_size_pixels);
		}

		Position new_player_pos_left  = new_player_pos;
		new_player_pos_left.point_x  -= player_width / 2;
		new_player_pos_left.normalize(state.world.tile_size_pixels);

		Position new_player_pos_right = new_player_pos;
		new_player_pos_right.point_x += player_width / 2;
		new_player_pos_right.normalize(state.world.tile_size_pixels);

		if (state.world.check_empty_tile(new_player_pos_left)
		 && state.world.check_empty_tile(new_player_pos)
		 && state.world.check_empty_tile(new_player_pos_right)) {
			state.player_pos = new_player_pos;
		}
		Scene current_scene = state.world.get_scene(state.player_pos);

		screen.draw_rectangle(Color{ 1.0f, 0.0f, 1.0f }, 0.0f, (f32)screen.width, 0.0f, (f32)screen.height, state.world.tile_size_pixels);
		for (i32 tile_y = 0; tile_y < Scene::HEIGHT; tile_y++) {
			for (i32 tile_x = 0; tile_x < Scene::WIDTH; tile_x++) {
				f32 min_x = tile_x * state.world.tile_size_pixels;
				f32 min_y = tile_y * state.world.tile_size_pixels;
				f32 max_x = min_x + state.world.tile_size_pixels;
				f32 max_y = min_y + state.world.tile_size_pixels;
				Color color = current_scene.tiles[tile_y][tile_x] ? Color{ 1.0f, 1.0f, 1.0f } : Color{ 0.5f, 0.5f, 0.5f };
				screen.draw_rectangle(color, min_x, max_x, min_y, max_y, state.world.tile_size_pixels);
			}
		}

		f32 player_min_x = state.player_pos.point_x - player_width / 2;
		f32 player_min_y = state.player_pos.point_y - player_height;
		f32 player_max_x = player_min_x + player_width;
		f32 player_max_y = player_min_y + player_height;
		screen.draw_rectangle(Color{ 1.0f, 0.0f, 0.0f }, player_min_x, player_max_x, player_min_y, player_max_y, state.world.tile_size_pixels);
	};

	bool World::check_empty_tile(const Position& position) {
		auto& world = *this;
		i32 tile_x = position.get_tile_x(world.tile_size_pixels);
		i32 tile_y = position.get_tile_y(world.tile_size_pixels);

		return tile_x >= 0 && tile_x < Scene::WIDTH  &&
		       tile_y >= 0 && tile_y < Scene::HEIGHT &&
	           world.get_scene(position).tiles[tile_y][tile_x] == 0;
	}

	void Position::normalize(f32 tile_size_pixels) {
		auto& position = *this;
		f32 scene_width_pixels  = Scene::WIDTH  * tile_size_pixels;
		f32 scene_height_pixels = Scene::HEIGHT * tile_size_pixels;

		if (position.point_x < 0 && position.scene_x - 1 >= 0) {
			position.scene_x -= 1;
			position.point_x += scene_width_pixels;
		} else if (position.point_x >= scene_width_pixels && position.scene_x + 1 < World::WIDTH) {
			position.scene_x += 1;
			position.point_x -= scene_width_pixels;
		}

		if (position.point_y < 0 && position.scene_y - 1 >= 0) {
			position.scene_y -= 1;
			position.point_y += scene_height_pixels;
		} else if (position.point_y >= scene_height_pixels && position.scene_y + 1 < World::HEIGHT) {
			position.scene_y += 1;
			position.point_y -= scene_height_pixels;
		}

		assert(position.point_x >= 0 && position.point_x < scene_width_pixels);
		assert(position.point_y >= 0 && position.point_y < scene_height_pixels);
	}

	void Screen::draw_rectangle(const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32, f32 tile_size_pixels) {
		auto& screen = *this;
		i32 render_offset_x = (i32)(- tile_size_pixels / 2);
		i32 render_offset_y = 0;

		i32 min_x = hm::round(min_x_f32) + render_offset_x;
		i32 max_x = hm::round(max_x_f32) + render_offset_x;
		i32 min_y = hm::round(min_y_f32) + render_offset_y;
		i32 max_y = hm::round(max_y_f32) + render_offset_y;

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

	void Screen::dev_draw_mouse_test(const Input& input, f32 tile_size_pixels) {
		if constexpr (!DEV_MODE) return;
		auto& screen = *this;
		screen.draw_rectangle(Color{ 1.0f, 1.0f, 1.0f }, (f32)input.dev_mouse.x, (f32)input.dev_mouse.x + 10.0f, (f32)input.dev_mouse.y, (f32)input.dev_mouse.y + 10.0f, tile_size_pixels);
		if (input.dev_mouse.left_button.is_pressed) screen.draw_rectangle(Color{ 1.0f, 1.0f, 1.0f }, 10.0f, 20.0f, 10.0f, 20.0f, tile_size_pixels);
		if (input.dev_mouse.right_button.is_pressed) screen.draw_rectangle(Color{ 1.0f, 1.0f, 1.0f }, 30.0f, 40.0f, 10.0f, 20.0f, tile_size_pixels);
	};

	Game_State& Memory::get_game_state() {
		auto& memory = *this;
		Game_State& state = *(Game_State*)memory.permanent_storage;
		if (memory.is_initialized) return state;

		i32 TILES_00[Scene::HEIGHT][Scene::WIDTH] = {
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 1,0,0,0,0, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
		};
		i32 TILES_01[Scene::HEIGHT][Scene::WIDTH] = {
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 0,0,0,0,1, 0,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 0,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
		};
		i32 TILES_10[Scene::HEIGHT][Scene::WIDTH] = {
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,0, 1,0,0,0,0, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
		};
		i32 TILES_11[Scene::HEIGHT][Scene::WIDTH] = {
			{ 1,1,1,1,1,1, 1,1,0,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 0,0,0,0,1, 0,0,0,0,0,1 },
			{ 0,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
		};

		hm::memcpy((void*)state.TILES_00, TILES_00, sizeof(TILES_00));
		hm::memcpy((void*)state.TILES_01, TILES_01, sizeof(TILES_01));
		hm::memcpy((void*)state.TILES_10, TILES_10, sizeof(TILES_10));
		hm::memcpy((void*)state.TILES_11, TILES_11, sizeof(TILES_11));

		Scene SCENES[World::HEIGHT][World::WIDTH] = {
			{ Scene{ state.TILES_00 }, Scene{ state.TILES_01 } },
			{ Scene{ state.TILES_10 }, Scene{ state.TILES_11 } },
		};
		hm::memcpy((Scene*)state.SCENES, SCENES, sizeof(SCENES));

		state.world.scenes = (Scene*)state.SCENES;
		state.world.tile_size_pixels = 60.0f;
		state.player_pos.point_x = 100.0f;
		state.player_pos.point_y = 100.0f;
		
		assert((i64)sizeof(Game_State) <= memory.permanent_size);
		assert(state.world.check_empty_tile(state.player_pos));
		
		memory.is_initialized = true;
		return state;
	}
}