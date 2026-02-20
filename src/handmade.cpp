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

		f32 tile_width  = 60.0f;
		f32 tile_height = 60.0f;
		i32 tile_map[9][17] = {
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },

			{ 1,0,0,0,0,1, 1,0,0,0,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			
			{ 1,0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0,1 },
			{ 1,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,1 },
			{ 1,1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,1 },
		};

		f32 player_width  = tile_width * 0.75f;
		f32 player_height = tile_height;
		f32 player_speed  = 200.0f;

		for (auto controller : input.controllers) {
			f32 player_dx = 0;
			f32 player_dy = 0;
			if (controller.move_left.is_pressed)  player_dx = - player_speed;
			if (controller.move_right.is_pressed) player_dx =   player_speed;
			if (controller.move_up.is_pressed)    player_dy = - player_speed;
			if (controller.move_down.is_pressed)  player_dy =   player_speed;
			game_state.player_x += player_dx * input.frame_dt;
			game_state.player_y += player_dy * input.frame_dt;
		}
		
		draw_rectangle(screen_buffer, 0.0f, (f32)screen_buffer.width, 0.0f, (f32)screen_buffer.height, Color{ 1.0f, 0.0f, 1.0f });
		for (i32 j = 0; j < hm::array_size(tile_map); j++) {
			for (i32 i = 0; i < hm::array_size(tile_map[j]); i++) {
				f32 min_x = i * tile_width - 30.0f;
				f32 min_y = j * tile_height;
				f32 max_x = min_x + tile_width;
				f32 max_y = min_y + tile_height;
				Color color = tile_map[j][i] ? Color{ 1.0f, 1.0f, 1.0f } : Color{ 0.5f, 0.5f, 0.5f };
				draw_rectangle(screen_buffer, min_x, max_x, min_y, max_y, color);
			}
		}
		f32 player_min_x = game_state.player_x - 0.5f * player_width;
		f32 player_min_y = game_state.player_y - player_height;
		f32 player_max_x = player_min_x + player_width;
		f32 player_max_y = player_min_y + player_height;
		draw_rectangle(screen_buffer, player_min_x, player_max_x, player_min_y, player_max_y, Color{ 1.0f, 0.0f, 0.0f });
	};

	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer) {
		Sound_Sample* sample = sound_buffer.samples;

		for (i32 i = 0; i < sound_buffer.samples_to_write; i++) {
			i16 value = 0;
			sample->left = value;
			sample->right = value;
			sample++;
		}
	}

	static void draw_rectangle(Screen_Buffer& screen_buffer, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32, Color color) {
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
		draw_rectangle(screen_buffer, (f32)input.dev_mouse.x, (f32)input.dev_mouse.x + 10.0f, (f32)input.dev_mouse.y, (f32)input.dev_mouse.y + 10.0f, Color{ 1.0f, 1.0f, 1.0f });
		if (input.dev_mouse.left_button.is_pressed) draw_rectangle(screen_buffer, 10.0f, 20.0f, 10.0f, 20.0f, Color{ 1.0f, 1.0f, 1.0f });
		if (input.dev_mouse.right_button.is_pressed) draw_rectangle(screen_buffer, 30.0f, 40.0f, 10.0f, 20.0f, Color{ 1.0f, 1.0f, 1.0f });
	};
}