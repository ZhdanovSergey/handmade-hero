#include "game.hpp"
#include "game_tiles.cpp"

namespace Game {
	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer) {
		Sound_Sample* sample = sound_buffer.samples;
		for (i32 i = 0; i < sound_buffer.samples_to_write; i++) {
			*sample = {};
			sample++;
		}
	}
	
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer) {
		assert((i64)sizeof(Game_State) <= memory.permanent_size);
		
		World world = {};
		world.width = WORLD_WIDTH;
		world.height = WORLD_HEIGHT;
		world.scene_width = SCENE_WIDTH;
		world.scene_height = SCENE_HEIGHT;
		world.tile_size = 60.0f;
		world.scenes = *SCENES;
		
		Game_State& game_state = *(Game_State*)memory.permanent_storage;
		if (!memory.is_initialized) {
			memory.is_initialized = true;
			game_state.player_pos.point_x = 100.0f;
			game_state.player_pos.point_y = 100.0f;
			assert(check_empty_tile(world, game_state.player_pos));
		}

		f32 player_speed  = 200.0f;
		f32 player_width  = world.tile_size * 0.75f;
		f32 player_height = world.tile_size;

		Position new_player_pos = game_state.player_pos;
		for (auto& controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy = - player_speed;
			if (controller.move_down.is_pressed)  player_dy =   player_speed;
			new_player_pos.point_x += player_dx * input.frame_dt;
			new_player_pos.point_y += player_dy * input.frame_dt;
			new_player_pos.normalize(world);
		}

		Position new_player_pos_left  = new_player_pos;
		new_player_pos_left.point_x  -= player_width / 2;
		new_player_pos_left.normalize(world);

		Position new_player_pos_right = new_player_pos;
		new_player_pos_right.point_x += player_width / 2;
		new_player_pos_right.normalize(world);

		if (check_empty_tile(world, new_player_pos_left) && check_empty_tile(world, new_player_pos) && check_empty_tile(world, new_player_pos_right)) {
			game_state.player_pos = new_player_pos;
		}
		Scene current_scene = world.get_scene(game_state.player_pos);

		draw_rectangle(screen_buffer, Color{ 1.0f, 0.0f, 1.0f }, 0.0f, (f32)screen_buffer.width, 0.0f, (f32)screen_buffer.height);
		for (i32 tile_y = 0; tile_y < world.scene_height; tile_y++) {
			for (i32 tile_x = 0; tile_x < world.scene_width; tile_x++) {
				f32 min_x = tile_x * world.tile_size;
				f32 min_y = tile_y * world.tile_size;
				f32 max_x = min_x + world.tile_size;
				f32 max_y = min_y + world.tile_size;
				Color color = current_scene.tiles[tile_y * world.scene_width + tile_x] ? Color{ 1.0f, 1.0f, 1.0f } : Color{ 0.5f, 0.5f, 0.5f };
				draw_rectangle(screen_buffer, color, min_x, max_x, min_y, max_y);
			}
		}

		f32 player_min_x = game_state.player_pos.point_x - player_width / 2;
		f32 player_min_y = game_state.player_pos.point_y - player_height;
		f32 player_max_x = player_min_x + player_width;
		f32 player_max_y = player_min_y + player_height;
		draw_rectangle(screen_buffer, Color{ 1.0f, 0.0f, 0.0f }, player_min_x, player_max_x, player_min_y, player_max_y);
	};

	static bool check_empty_tile(const World& world, const Position& position) {
		i32 tile_x = position.get_tile_x(world.tile_size);
		i32 tile_y = position.get_tile_y(world.tile_size);

		return tile_x >= 0 && tile_x < world.scene_width  &&
		       tile_y >= 0 && tile_y < world.scene_height &&
	           world.get_scene(position).tiles[tile_y * world.scene_width + tile_x] == 0;
	}

	void Position::normalize(const World& world) {
		auto& position = *this;
		f32 scene_width_pixels  = world.get_scene_width_pixels();
		f32 scene_height_pixels = world.get_scene_height_pixels();

		if (position.point_x < 0 && position.scene_x - 1 >= 0) {
			position.scene_x -= 1;
			position.point_x += scene_width_pixels;
		} else if (position.point_x >= scene_width_pixels && position.scene_x + 1 < world.width) {
			position.scene_x += 1;
			position.point_x -= scene_width_pixels;
		}

		if (position.point_y < 0 && position.scene_y - 1 >= 0) {
			position.scene_y -= 1;
			position.point_y += scene_height_pixels;
		} else if (position.point_y >= scene_height_pixels && position.scene_y + 1 < world.height) {
			position.scene_y += 1;
			position.point_y -= scene_height_pixels;
		}

		assert(position.point_x >= 0 && position.point_x < scene_width_pixels);
		assert(position.point_y >= 0 && position.point_y < scene_height_pixels);
	}

	static void draw_rectangle(Screen_Buffer& screen_buffer, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32) {
		i32 render_offset_x = -30;
		i32 render_offset_y = 0;

		// TODO: сейчас сцена занимает больше места по горизонтали, чем отображается на экране
		i32 min_x = hm::round(min_x_f32) + render_offset_x;
		i32 max_x = hm::round(max_x_f32) + render_offset_x;
		i32 min_y = hm::round(min_y_f32) + render_offset_y;
		i32 max_y = hm::round(max_y_f32) + render_offset_y;

		min_x = hm::max(min_x, 0);
		max_x = hm::min(max_x, screen_buffer.width);
		min_y = hm::max(min_y, 0);
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